// charart_core.h
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <limits>
#include <random>

// ---------- 色彩 ----------
static inline float srgb_to_linear(float c) {
    return (c <= 0.04045f) ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
static inline float linear_to_srgb(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return (c <= 0.0031308f) ? c * 12.92f : 1.055f * std::pow(c, 1.0f/2.4f) - 0.055f;
}

// ---------- 8x8 点阵字体（ASCII 32..126）----------
static const unsigned char FONT8X8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x18,0x18,0x18,0x18,0x00,0x00,0x18,0x00},
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00},{0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00},{0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00},{0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00},{0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},{0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06},{0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00},{0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00},{0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00},{0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00},{0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00},{0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00},{0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00},{0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00},{0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},{0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00},{0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00},{0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00},{0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00},{0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00},{0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00},{0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00},{0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00},
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00},{0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00},{0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00},{0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},{0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00},{0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    {0x63,0x63,0x36,0x1C,0x36,0x63,0x63,0x00},{0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},{0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00},
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},{0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00},
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00},
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00},{0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00},
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00},{0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00},
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00},{0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F},
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00},{0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E},{0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00},
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},{0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00},
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00},{0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00},
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F},{0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78},
    {0x00,0x00,0x3B,0x6E,0x06,0x06,0x0F,0x00},{0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00},
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00},{0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00},{0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00},
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00},{0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F},
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00},{0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00},
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},{0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00},
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},
};

static const char* CAND_CHARS = " .:-=+*#%@|/\\_~<>^";
static const int NCAND_RAW = 20;

struct Glyph {
    unsigned char pix[8][8];
    float ink;
    float sub4x4[16];
};

static Glyph make_glyph(char c) {
    Glyph g{};
    int idx = (int)c - 32;
    if (idx < 0 || idx >= 95) std::memset(g.pix, 0, sizeof(g.pix));
    else for (int y=0;y<8;++y) for (int x=0;x<8;++x)
        g.pix[y][x] = (FONT8X8[idx][y] >> (7-x)) & 1;
    int cnt=0; for(int y=0;y<8;++y) for(int x=0;x<8;++x) cnt+=g.pix[y][x];
    g.ink = cnt/64.0f;
    for (int sy=0;sy<4;++sy) for (int sx=0;sx<4;++sx) {
        int s=0; for(int y=0;y<2;++y) for(int x=0;x<2;++x) s+=g.pix[sy*2+y][sx*2+x];
        g.sub4x4[sy*4+sx] = s/4.0f;
    }
    return g;
}

// ---------- 预计算表（进程内只读一次）----------
struct CharArtTables {
    std::vector<char>                 chars;
    std::vector<std::array<float,16>> vis4;
    int NC = 0;
};

static const CharArtTables& get_charart_tables() {
    static const CharArtTables T = []{
        CharArtTables t;
        struct Cand { char c; Glyph g; };
        std::vector<Cand> cands;
        for (int i=0;i<NCAND_RAW;++i) {
            char c=CAND_CHARS[i]; if(!c) break;
            cands.push_back({c, make_glyph(c)});
        }
        std::sort(cands.begin(),cands.end(),
            [](const Cand&a,const Cand&b){return a.g.ink>b.g.ink;});
        t.NC=(int)cands.size();
        t.chars.resize(t.NC);
        t.vis4.resize(t.NC);
        for (int i=0;i<t.NC;++i){
            t.chars[i]=cands[i].c;
            for (int k=0;k<16;++k)
                t.vis4[i][k]=1.0f-cands[i].g.sub4x4[k];
        }
        return t;
    }();
    return T;
}

// ---------- 搜索强度 ----------
enum class SearchQuality { Fast=0, Normal=1, High=2, Ultra=3 };
struct SearchParams {
    int initTrials;   // 初始候选采样次数
    int refineIters;  // 块内精修轮数
    int jointPasses;  // 块间联合优化轮数
    int jointTrials;  // 每块联合优化的随机试验次数
};
static inline SearchParams get_search_params(SearchQuality q){
    switch(q){
        case SearchQuality::Fast:   return {1,1,0,0};
        case SearchQuality::Normal: return {2,2,1,1};
        case SearchQuality::High:   return {3,4,2,2};
        case SearchQuality::Ultra:  return {6,8,4,3};
    }
    return {1,1,0,0};
}

// ---------- 线程局部缓冲 ----------
struct FrameWorkBuf {
    std::vector<float>              lin;
    std::vector<float>              ds;
    std::vector<float>              ds2;
    std::vector<float>              tmpH;
    std::vector<float>              target;
    std::vector<std::array<int,4>>  choice;
    std::mt19937                    rng{12345u};
};

// 区域平均降采样（逐步减半 + 面积加权），复用缓冲
static void area_downsample_gray_into(
    const std::vector<float>& gray, int w, int h, int gw, int gh,
    std::vector<float>& out, FrameWorkBuf& buf)
{
    std::vector<float>* cur = &buf.ds;
    std::vector<float>* nxt = &buf.ds2;
    cur->assign(gray.begin(), gray.end());
    int cw=w, ch=h;
    while (cw >= gw*2 && ch >= gh*2 && cw>2 && ch>2) {
        int nw=cw/2, nh=ch/2;
        nxt->resize((size_t)nw*nh);
        for (int y=0;y<nh;++y) for (int x=0;x<nw;++x) {
            float s = (*cur)[(size_t)(2*y)*cw+2*x] + (*cur)[(size_t)(2*y)*cw+2*x+1]
                    + (*cur)[(size_t)(2*y+1)*cw+2*x] + (*cur)[(size_t)(2*y+1)*cw+2*x+1];
            (*nxt)[(size_t)y*nw+x] = s*0.25f;
        }
        std::swap(cur, nxt); cw=nw; ch=nh;
    }
    // 水平
    std::vector<float>& tmp = buf.tmpH;
    tmp.resize((size_t)ch*gw);
    for (int y=0;y<ch;++y) for (int gx=0;gx<gw;++gx) {
        float a=(float)gx*cw/gw, b=(float)(gx+1)*cw/gw, sum=0.f, ws=0.f;
        int i0=(int)std::floor(a), i1=(int)std::ceil(b);
        for (int i=std::max(0,i0); i<std::min(cw,i1); ++i) {
            float l=std::max(a,(float)i), r=std::min(b,(float)(i+1));
            float wgt=std::max(0.f,r-l);
            sum += (*cur)[(size_t)y*cw+i]*wgt; ws += wgt;
        }
        tmp[(size_t)y*gw+gx] = ws>0 ? sum/ws : 0.f;
    }
    // 垂直
    out.resize((size_t)gw*gh);
    for (int gy=0;gy<gh;++gy) {
        float a=(float)gy*ch/gh, b=(float)(gy+1)*ch/gh;
        for (int gx=0;gx<gw;++gx) {
            float sum=0.f, ws=0.f;
            int j0=(int)std::floor(a), j1=(int)std::ceil(b);
            for (int j=std::max(0,j0); j<std::min(ch,j1); ++j) {
                float l=std::max(a,(float)j), r=std::min(b,(float)(j+1));
                float wgt=std::max(0.f,r-l);
                sum += tmp[(size_t)j*gw+gx]*wgt; ws += wgt;
            }
            out[(size_t)gy*gw+gx] = ws>0 ? sum/ws : 0.f;
        }
    }
}

// ---------- 核心：单帧转字符索引（_into 版本）----------
static void frame_to_charindex_into(
    const unsigned char* rgb, int w, int h, int cols, int rows,
    float gain, float gamma, bool invert,
    SearchQuality quality,
    FrameWorkBuf& buf, std::vector<unsigned char>& outGrid)
{
    const CharArtTables& T = get_charart_tables();
    const int NC = T.NC;
    const auto& vis4 = T.vis4;
    const SearchParams SP = get_search_params(quality);

    // 转线性灰度
    buf.lin.resize((size_t)w*h);
    for (size_t i=0;i<(size_t)w*h;++i) {
        float r=srgb_to_linear(rgb[i*3+0]/255.0f);
        float g=srgb_to_linear(rgb[i*3+1]/255.0f);
        float b=srgb_to_linear(rgb[i*3+2]/255.0f);
        buf.lin[i]=r*0.2126f+g*0.7152f+b*0.0722f;
    }
    int gw=cols*4, gh=rows*4;

    std::vector<float> dsResult;
    area_downsample_gray_into(buf.lin, w, h, gw, gh, dsResult, buf);
    std::vector<float>& target = dsResult;

    for (auto& v : target) {
        v = std::clamp(v*gain,0.0f,1.0f);
        float s = linear_to_srgb(v);
        s = std::pow(s,gamma);
        v = srgb_to_linear(s);
    }
    if (invert) for (auto& v : target) v = 1.0f - v;

    auto block_err = [&](int bx,int by,const int idx[4])->float{
        float err=0.f;
        for (int cy=0;cy<2;++cy) for (int cx=0;cx<2;++cx) {
            int ci=idx[cy*2+cx];
            for (int sy=0;sy<2;++sy) for (int sx=0;sx<2;++sx) {
                int gx=bx*4+cx*2+sx, gy=by*4+cy*2+sy;
                float t=target[(size_t)gy*gw+gx];
                float v=vis4[ci][(cy*2+sy)*4+(cx*2+sx)];
                float e=v-t; err+=e*e;
            }
        }
        return err;
    };

    buf.choice.resize((size_t)cols*rows);
    std::vector<std::array<int,4>>& choice = buf.choice;
    std::mt19937& rng = buf.rng;

    for (int by=0;by<rows;++by) for (int bx=0;bx<cols;++bx) {
        std::array<int,4> best{}; float bestE=1e30f;
        // 初始采样
        for (int s=0;s<SP.initTrials;++s) {
            int idx[4];
            if (s==0) {
                for (int k=0;k<4;++k) {
                    int cx=k%2, cy=k/2; float tsum=0.f;
                    for (int sy=0;sy<2;++sy) for (int sx=0;sx<2;++sx)
                        tsum += target[(size_t)(by*4+cy*2+sy)*gw+(bx*4+cx*2+sx)];
                    tsum*=0.25f;
                    int bi=0; float be=1e9f;
                    for (int i=0;i<NC;++i) {
                        float v=0.f;
                        for (int sy=0;sy<2;++sy) for (int sx=0;sx<2;++sx)
                            v += vis4[i][(cy*2+sy)*4+(cx*2+sx)];
                        v*=0.25f; float e=(v-tsum)*(v-tsum);
                        if(e<be){be=e;bi=i;}
                    }
                    idx[k]=bi;
                }
            } else for (int k=0;k<4;++k) idx[k]=rng()%NC;
            float e=block_err(bx,by,idx);
            if(e<bestE){bestE=e;best={idx[0],idx[1],idx[2],idx[3]};}
        }
        // 精修
        for (int iter=0;iter<SP.refineIters;++iter) {
            bool ch=false;
            for (int k=0;k<4;++k) {
                int cur[4]={best[0],best[1],best[2],best[3]};
                int bi=cur[k]; float be=block_err(bx,by,cur);
                for (int i=0;i<NC;++i) {
                    if(i==cur[k])continue;
                    cur[k]=i; float e=block_err(bx,by,cur);
                    if(e<be){be=e;bi=i;ch=true;}
                }
                cur[k]=bi; best[k]=bi;
            }
            if(!ch)break;
        }
        choice[(size_t)by*cols+bx]=best;
    }

    // 块间联合优化（可能 0 轮）
    if (SP.jointPasses>0) {
        auto neighbor_smooth=[&](int bx,int by,const int idx[4])->float{
            float e=0.f;
            if(bx+1<cols){
                const auto& nb=choice[(size_t)by*cols+(bx+1)];
                for(int cy=0;cy<2;++cy) for(int sy=0;sy<2;++sy){
                    float a=vis4[idx[cy*2+1]][(cy*2+sy)*4+3];
                    float b=vis4[nb[cy*2+0]][(cy*2+sy)*4+0];
                    e+=(a-b)*(a-b);
                }
            }
            if(by+1<rows){
                const auto& nb=choice[(size_t)(by+1)*cols+bx];
                for(int cx=0;cx<2;++cx) for(int sx=0;sx<2;++sx){
                    float a=vis4[idx[2+cx]][(2+0)*4+(cx*2+sx)];
                    float b=vis4[nb[0+cx]][(0+1)*4+(cx*2+sx)];
                    e+=(a-b)*(a-b);
                }
            }
            return e;
        };
        const float LAMBDA=0.15f;
        for(int pass=0;pass<SP.jointPasses;++pass){
            bool any=false;
            for(int by=0;by<rows;++by) for(int bx=0;bx<cols;++bx){
                auto& cur=choice[(size_t)by*cols+bx];
                float curE=block_err(bx,by,cur.data())+LAMBDA*neighbor_smooth(bx,by,cur.data());
                std::array<int,4> best=cur; float bestE=curE;
                for(int trial=0;trial<SP.jointTrials;++trial){
                    std::array<int,4> cand=cur;
                    if(trial==0){for(int k=0;k<4;++k)cand[k]=rng()%NC;}
                    else{for(int k=0;k<4;++k)if(rng()%2)cand[k]=rng()%NC;}
                    for(int iter=0;iter<1;++iter){
                        bool ch=false;
                        for(int k=0;k<4;++k){
                            int bi=cand[k];
                            float be=block_err(bx,by,cand.data())+LAMBDA*neighbor_smooth(bx,by,cand.data());
                            for(int i=0;i<NC;++i){
                                if(i==cand[k])continue;
                                cand[k]=i;
                                float e=block_err(bx,by,cand.data())+LAMBDA*neighbor_smooth(bx,by,cand.data());
                                if(e<be){be=e;bi=i;ch=true;}
                            }
                            cand[k]=bi;
                        }
                        if(!ch)break;
                    }
                    float e=block_err(bx,by,cand.data())+LAMBDA*neighbor_smooth(bx,by,cand.data());
                    if(e<bestE){bestE=e;best=cand;}
                }
                if(best!=cur){cur=best;any=true;}
            }
            if(!any)break;
        }
    }

    // 写输出
    outGrid.resize((size_t)(rows*2)*(cols*2));
    for(int by=0;by<rows;++by) for(int bx=0;bx<cols;++bx){
        const auto& idx=choice[(size_t)by*cols+bx];
        outGrid[(size_t)(by*2+0)*(cols*2)+(bx*2+0)] = (unsigned char)idx[0];
        outGrid[(size_t)(by*2+0)*(cols*2)+(bx*2+1)] = (unsigned char)idx[1];
        outGrid[(size_t)(by*2+1)*(cols*2)+(bx*2+0)] = (unsigned char)idx[2];
        outGrid[(size_t)(by*2+1)*(cols*2)+(bx*2+1)] = (unsigned char)idx[3];
    }
}

// ---------- 兼容旧接口 ----------
static std::vector<unsigned char> frame_to_charindex(
    const unsigned char* rgb, int w, int h, int cols, int rows,
    float gain, float gamma, bool invert,
    SearchQuality quality = SearchQuality::Normal)
{
    FrameWorkBuf buf;
    std::vector<unsigned char> out;
    frame_to_charindex_into(rgb,w,h,cols,rows,gain,gamma,invert,quality,buf,out);
    return out;
}