// mp4_to_dat.cpp —— 完整定稿
// 1) 输出 CART v1（HTML 播放器可直接读：CART/ver1/u16 W,H/f32 fps/u8 nch/charset/u32 frameCount/3字节RLE）
// 2) 进度面板：无定时线程，帧算完立即原地覆写（ANSI 4行 / 降级单行 \r），防闪防滚屏
// 3) read_full 循环读满、快速 probe_nframes、--preview、--quality、线程并行
#include "charart_core.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#define popen  _popen
#define pclose _pclose
#include <windows.h>
#else
#include <unistd.h>
#endif

// ---------------- 基础 IO ----------------
static size_t read_full(FILE* fp, unsigned char* buf, size_t n){
    size_t got=0;
    while(got<n){ size_t r=fread(buf+got,1,n-got,fp); if(r==0)break; got+=r; }
    return got;
}
static void write_u32le(FILE* f, uint32_t v){
    unsigned char b[4]={(unsigned char)(v&0xFF),(unsigned char)((v>>8)&0xFF),
                        (unsigned char)((v>>16)&0xFF),(unsigned char)((v>>24)&0xFF)};
    fwrite(b,1,4,f);
}
static void write_u16le(FILE* f, uint16_t v){
    unsigned char b[2]={(unsigned char)(v&0xFF),(unsigned char)((v>>8)&0xFF)};
    fwrite(b,1,2,f);
}
static void write_f32le(FILE* f, float v){
    unsigned char b[4]; std::memcpy(b,&v,4); fwrite(b,1,4,f);
}

// ---------------- 快速探测总帧数 ----------------
static long long probe_nframes(const char* path, double fps_hint){
    { char cmd[1024];
      std::snprintf(cmd,sizeof(cmd),
        "ffprobe -v error -select_streams v:0 -show_entries stream=nb_frames "
        "-of default=noprint_wrappers=1:nokey=1 \"%s\"", path);
      FILE* p=popen(cmd,"r");
      if(p){ char buf[64]={0}; if(fgets(buf,sizeof(buf),p)){} pclose(p);
             long long v=atoll(buf); if(v>0) return v; } }
    if(fps_hint>0){
      char cmd[1024];
      std::snprintf(cmd,sizeof(cmd),
        "ffprobe -v error -select_streams v:0 -show_entries format=duration "
        "-of default=noprint_wrappers=1:nokey=1 \"%s\"", path);
      FILE* p=popen(cmd,"r");
      if(p){ char buf[64]={0}; if(fgets(buf,sizeof(buf),p)){} pclose(p);
             double dur=atof(buf); if(dur>0) return (long long)(dur*fps_hint+0.5); }
    }
    return 0;
}

// ---------------- 零延迟原地覆写进度面板 ----------------
struct Progress {
    std::string inPath,outPath,qualityName;
    int srcw=0,srch=0,cols=0,rows=0,nthreads=0,batchN=0;
    float gain=1.0f,gamma=1.0f; bool invert=false;
    long long totalFrames=0;

    std::atomic<uint32_t> calcFrames{0},writeFrames{0};
    std::atomic<uint64_t> bytesWritten{0};
    std::atomic<int> activeWorkers{0};

    std::chrono::steady_clock::time_point t0{};
    uint32_t lastCalc=0; double smoothCalcFps=0;
    bool enabled=true, useAnsi=false;
    int termW=80;
    int panelRows=4;
    std::atomic<int64_t> lastDrawNs{0};
    std::mutex drawMtx;

    static std::string hms(double s){ if(s<0)s=0; int x=(int)(s+0.5); char b[32];
        std::snprintf(b,sizeof(b),"%02d:%02d:%02d",x/3600,(x/60)%60,x%60); return b; }
    static void make_bar(char* o,int w,double p){ if(p<0)p=0; if(p>100)p=100;
        int f=(int)(p/100.0*w); if(f>w)f=w; for(int i=0;i<w;++i)o[i]=(i<f)?'#':'-'; o[w]='\0'; }

    void pad_trunc(char* buf,size_t cap) const {
        size_t len=strlen(buf);
        int limit=termW>2?termW-1:79;
        if((int)len>limit){ buf[limit]='\0'; len=(size_t)limit; }
        while((int)len<limit && len+1<cap) buf[len++]=' ';
        buf[len]='\0';
    }

    void start(){
        if(!enabled) return;
#ifdef _WIN32
        HANDLE h=GetStdHandle(STD_ERROR_HANDLE);
        DWORD mode=0;
        if(h && h!=INVALID_HANDLE_VALUE && GetConsoleMode(h,&mode)){
            if(SetConsoleMode(h,mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING)) useAnsi=true;
            CONSOLE_SCREEN_BUFFER_INFO info;
            if(GetConsoleScreenBufferInfo(h,&info)){
                termW=info.srWindow.Right-info.srWindow.Left+1;
                if(termW<20) termW=80;
            }
        }
#endif
        t0=std::chrono::steady_clock::now();
        std::fprintf(stderr,
            "--------------------------------------------------------\n"
            " input : %s (%dx%d)\n"
            " output: %s (grid %dx%d -> pixel %dx%d)\n"
            " params: gain=%.2f gamma=%.2f invert=%s quality=%s threads=%d batch=%d\n"
            "--------------------------------------------------------\n",
            inPath.c_str(),srcw,srch,outPath.c_str(),cols,rows,cols*2,rows*2,
            gain,gamma,invert?"yes":"no",qualityName.c_str(),nthreads,batchN);
        if(useAnsi){
            std::fprintf(stderr,"\x1b[?25l\n\n\n\n\x1b[4A"); // 隐藏光标 + 预留 4 行 + 回顶
            std::fflush(stderr);
        }
        lastDrawNs.store(0);
    }

    void notifyFrame(){
        if(!enabled) return;
        auto now=std::chrono::steady_clock::now();
        int64_t ns=std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        int64_t last=lastDrawNs.load(std::memory_order_relaxed);
        if(ns-last < 30'000'000) return;
        if(!lastDrawNs.compare_exchange_strong(last,ns,std::memory_order_relaxed)) return;
        draw();
    }

    void finish(){
        if(!enabled) return;
        draw();
        if(useAnsi){
            std::fprintf(stderr,"\x1b[%dB\x1b[?25h\n", panelRows); // 下移到面板下方 + 恢复光标
        } else {
            std::fprintf(stderr,"\n");
        }
        std::fflush(stderr);
    }

    void draw(){
        std::lock_guard<std::mutex> lk(drawMtx);
        auto now=std::chrono::steady_clock::now();
        double sec=std::chrono::duration<double>(now-t0).count(); if(sec<=0)sec=1e-6;
        uint32_t c=calcFrames.load(std::memory_order_relaxed);
        uint32_t w=writeFrames.load(std::memory_order_relaxed);
        uint64_t bw=bytesWritten.load(std::memory_order_relaxed);
        double inst=(c-lastCalc)/0.25; smoothCalcFps=smoothCalcFps*0.7+inst*0.3; lastCalc=c;
        double pct=totalFrames>0?100.0*c/totalFrames:-1;
        char bar[80]; make_bar(bar,35,pct);
        double eta=(totalFrames>0&&smoothCalcFps>1e-3)?(totalFrames-c)/smoothCalcFps:-1;
        double memGB=(double)bw/(1024.0*1024.0*1024.0);
        int act=activeWorkers.load(std::memory_order_relaxed);

        char l1[256],l2[256],l3[256],l4[256];
        if(totalFrames>0){
            std::snprintf(l1,sizeof(l1)," calc [%s] %5.1f%%  %u/%lld  %6.1f fps",
                          bar,pct,c,totalFrames,smoothCalcFps);
            std::snprintf(l2,sizeof(l2)," writ %6u/%lld  %6.1f%%",
                          w,totalFrames,totalFrames>0?100.0*w/totalFrames:0);
        } else {
            std::snprintf(l1,sizeof(l1)," calc [%s]  %u frames  %6.1f fps",
                          bar,c,smoothCalcFps);
            std::snprintf(l2,sizeof(l2)," writ %6u frames",w);
        }
        std::snprintf(l3,sizeof(l3)," mem  %6.2f GB   workers %d/%d", memGB,act,nthreads);
        std::snprintf(l4,sizeof(l4)," time %s   ETA %s",
                      hms(sec).c_str(), eta>0?hms(eta).c_str():"--:--:--");

        if(useAnsi){
            pad_trunc(l1,sizeof(l1)); pad_trunc(l2,sizeof(l2));
            pad_trunc(l3,sizeof(l3)); pad_trunc(l4,sizeof(l4));
            std::fprintf(stderr,"\x1b[4A"
                "\x1b[2K%s\n\x1b[2K%s\n\x1b[2K%s\n\x1b[2K%s",
                l1,l2,l3,l4);
        } else {
            char one[512];
            std::snprintf(one,sizeof(one),"%s | %s | %s | %s", l1,l2,l3,l4);
            pad_trunc(one,sizeof(one));
            std::fprintf(stderr,"\r%s", one);
        }
        std::fflush(stderr);
    }
};

// ---------------- 主流程 ----------------
int main(int argc, char** argv){
    if(argc<7){
        std::fprintf(stderr,
            "usage: %s input.mp4 out.dat cols rows gain gamma "
            "[--invert] [--threads N] [--batch N] "
            "[--quality fast|normal|high|ultra] [--preview N] [--fps F] [--no-progress]\n"
            "  strict 4K output: cols=1920 rows=1080\n", argv[0]);
        return 1;
    }
    const char* in=argv[1]; const char* outpath=argv[2];
    int cols=atoi(argv[3]); int rows=atoi(argv[4]);
    float gain=(float)atof(argv[5]); float gamma=(float)atof(argv[6]);
    bool invert=false;
    int nthreads=(int)std::thread::hardware_concurrency(); if(nthreads<1)nthreads=1;
    int batchN=100;
    SearchQuality quality=SearchQuality::Normal; std::string qn="normal";
    long long previewN=-1; double fpsHint=0.0; bool noProgress=false;
    for(int i=7;i<argc;++i){
        if(!std::strcmp(argv[i],"--invert")) invert=true;
        else if(!std::strcmp(argv[i],"--threads")&&i+1<argc) nthreads=atoi(argv[++i]);
        else if(!std::strcmp(argv[i],"--batch")&&i+1<argc) batchN=atoi(argv[++i]);
        else if(!std::strcmp(argv[i],"--no-progress")) noProgress=true;
        else if(!std::strcmp(argv[i],"--fps")&&i+1<argc) fpsHint=atof(argv[++i]);
        else if(!std::strcmp(argv[i],"--preview")&&i+1<argc) previewN=atoll(argv[++i]);
        else if(!std::strcmp(argv[i],"--quality")&&i+1<argc){
            std::string q=argv[++i];
            if(q=="fast"){quality=SearchQuality::Fast;qn="fast";}
            else if(q=="normal"){quality=SearchQuality::Normal;qn="normal";}
            else if(q=="high"){quality=SearchQuality::High;qn="high";}
            else if(q=="ultra"){quality=SearchQuality::Ultra;qn="ultra";}
            else { std::fprintf(stderr,"bad quality: %s\n",q.c_str()); return 1; }
        }
    }

    // 探源分辨率 / fps
    int srcw=0,srch=0; double srcFps=0;
    { char cmd[1024];
      std::snprintf(cmd,sizeof(cmd),
        "ffprobe -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate "
        "-of default=noprint_wrappers=1 \"%s\"", in);
      FILE* p=popen(cmd,"r");
      if(p){ char line[256];
        while(fgets(line,sizeof(line),p)){
            if(sscanf(line,"width=%d",&srcw)==1){}
            else if(sscanf(line,"height=%d",&srch)==1){}
            else { int n=0,d=0;
                if(sscanf(line,"r_frame_rate=%d/%d",&n,&d)==2&&d>0) srcFps=(double)n/d; }
        } pclose(p); } }
    if(srcw<=0||srch<=0){ std::fprintf(stderr,"probe failed\n"); return 1; }
    if(fpsHint<=0) fpsHint=srcFps>0?srcFps:30.0;

    const int OUT_W=cols*2, OUT_H=rows*2;
    if(OUT_W>65535||OUT_H>65535){
        std::fprintf(stderr,"grid too large: OUT_W=%d OUT_H=%d (u16 limit)\n",OUT_W,OUT_H);
        return 1;
    }
    std::fprintf(stderr,"[info] source %dx%d @ %.3f fps -> grid %dx%d -> output %dx%d\n",
        srcw,srch,fpsHint,cols,rows,OUT_W,OUT_H);
    if(OUT_W!=3840||OUT_H!=2160)
        std::fprintf(stderr,"[warn] output %dx%d != 4K(3840x2160); strict 4K: cols=1920 rows=1080\n",
            OUT_W,OUT_H);

    long long totalFrames=noProgress?0:probe_nframes(in,fpsHint);
    if(previewN>=0 && (totalFrames<=0||previewN<totalFrames)) totalFrames=previewN;

    // ffmpeg 解码管道
    char cmd[2048];
    std::snprintf(cmd,sizeof(cmd),"ffmpeg -v error -i \"%s\" -f rawvideo -pix_fmt rgb24 -",in);
#ifdef _WIN32
    FILE* pipe=_popen(cmd,"rb");
#else
    FILE* pipe=popen(cmd,"r");
#endif
    if(!pipe){ std::fprintf(stderr,"ffmpeg pipe fail\n"); return 1; }
    FILE* fout=fopen(outpath,"wb");
    if(!fout){ std::fprintf(stderr,"out fail\n"); return 1; }

    // ---------- 写 CART v1 头 ----------
    const CharArtTables& T = get_charart_tables();
    write_u32le(fout,0x54524143);      // "CART" 小端
    write_u32le(fout,1);               // version 1
    write_u16le(fout,(uint16_t)OUT_W);
    write_u16le(fout,(uint16_t)OUT_H);
    write_f32le(fout,(float)fpsHint);
    fputc((int)T.NC,fout);             // nch
    fwrite(T.chars.data(),1,T.NC,fout);// charset
    write_u32le(fout,(uint32_t)(totalFrames>0?totalFrames:0));

    const size_t frameBytes=(size_t)srcw*srch*3;
    std::vector<unsigned char> rgb(frameBytes);

    Progress prog;
    prog.inPath=in; prog.outPath=outpath; prog.srcw=srcw; prog.srch=srch;
    prog.cols=cols; prog.rows=rows; prog.gain=gain; prog.gamma=gamma;
    prog.invert=invert; prog.nthreads=nthreads; prog.batchN=batchN;
    prog.totalFrames=totalFrames; prog.qualityName=qn; prog.enabled=!noProgress;
    prog.start();

    std::vector<std::vector<unsigned char>> batchRgb; batchRgb.reserve(batchN);
    uint64_t frameCount=0;

    auto flushBatch=[&](){
        int n=(int)batchRgb.size(); if(n==0)return;
        std::vector<std::vector<unsigned char>> grids(n);
        std::atomic<int> next{0};
        std::vector<std::thread> workers;
        for(int t=0;t<nthreads;++t){
            workers.emplace_back([&](){
                FrameWorkBuf wb;
                while(true){
                    int i=next.fetch_add(1); if(i>=n)break;
                    prog.activeWorkers.fetch_add(1);
                    frame_to_charindex_into(batchRgb[i].data(),srcw,srch,cols,rows,
                        gain,gamma,invert,quality,wb,grids[i]);
                    prog.activeWorkers.fetch_sub(1);
                    prog.calcFrames.fetch_add(1,std::memory_order_relaxed);
                    prog.notifyFrame();      // 算完立即刷新面板
                }
            });
        }
        for(auto&t:workers)t.join();

        for(int i=0;i<n;++i){
            // 3 字节 RLE：u8 charIdx + u16 run(LE)
            std::vector<unsigned char> rle;
            rle.reserve(grids[i].size()/2);
            size_t k=0;
            while(k<grids[i].size()){
                unsigned char v=grids[i][k];
                size_t run=1;
                while(k+run<grids[i].size() && grids[i][k+run]==v && run<65535) ++run;
                rle.push_back(v);
                rle.push_back((unsigned char)(run & 0xFF));
                rle.push_back((unsigned char)((run>>8)&0xFF));
                k+=run;
            }
            write_u32le(fout,(uint32_t)rle.size());
            fwrite(rle.data(),1,rle.size(),fout);
            prog.bytesWritten.fetch_add(4+rle.size(),std::memory_order_relaxed);
            ++frameCount;
            prog.writeFrames.store((uint32_t)frameCount,std::memory_order_relaxed);
            prog.notifyFrame();              // 写完一帧也刷
        }
        batchRgb.clear();
    };

    while(true){
        size_t rd=read_full(pipe,rgb.data(),frameBytes);
        if(rd<frameBytes)break;              // 真正 EOF
        batchRgb.emplace_back(rgb.begin(),rgb.end());
        if((int)batchRgb.size()>=batchN) flushBatch();
        if(previewN>=0 && (long long)frameCount>=(long long)previewN) break;
    }
    flushBatch();
    prog.finish();

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    fclose(fout);
    std::fprintf(stderr,"[done] %llu frames -> %s\n",(unsigned long long)frameCount,outpath);
    return 0;
}