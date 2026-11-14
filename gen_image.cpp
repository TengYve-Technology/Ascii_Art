// charart_opt.cpp
// 编译: g++ -O2 -std=c++17 charart_opt.cpp -o charart_opt
// 用法: ./charart_opt input.png cols rows [gain] [gamma] [--invert]
//   cols/rows = 字符块数（每块 2x2 字符，输出字符网格 2*cols x 2*rows）
//   每个字符块对应 4x4 子像素目标（每字符占 2x2 子像素）

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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ============================================================
// 1. 基础色彩工具
// ============================================================
static inline float srgb_to_linear(float c) {
    return (c <= 0.04045f) ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
static inline float linear_to_srgb(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return (c <= 0.0031308f) ? c * 12.92f : 1.055f * std::pow(c, 1.0f/2.4f) - 0.055f;
}

// ============================================================
// 2. 内置 8x8 点阵字体（ASCII 32..126），真实字形栅格
//    每字符 8 字节，每字节一行，bit7..bit0 = 左到右 8 像素
//    这是经典 8x8 字体表，覆盖可见 ASCII，保证字形真实、方向性真实
// ============================================================
static const unsigned char FONT8X8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x18,0x18,0x18,0x18,0x00,0x00,0x18,0x00}, // '!'
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // '"'
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // '#'
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // '$'
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // '%'
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // '&'
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '\''
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // '('
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // '*'
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // '+'
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // ','
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // '-'
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // '.'
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // '/'
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // '0'
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // '1'
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // '2'
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // '3'
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // '4'
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // '5'
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // '6'
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // '7'
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // '8'
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // '9'
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // ':'
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // ';'
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // '<'
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // '='
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // '>'
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // '?'
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // '@'
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // 'A'
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // 'B'
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // 'C'
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // 'D'
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // 'E'
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // 'F'
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // 'G'
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // 'H'
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'I'
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // 'J'
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // 'K'
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // 'L'
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // 'M'
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // 'N'
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // 'O'
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // 'P'
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // 'Q'
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // 'R'
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // 'S'
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'T'
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // 'U'
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 'V'
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 'W'
    {0x63,0x63,0x36,0x1C,0x36,0x63,0x63,0x00}, // 'X'
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // 'Y'
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // 'Z'
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // '['
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // '\\'
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // ']'
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // '_'
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // '`'
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // 'a'
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // 'b'
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // 'c'
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, // 'd'
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, // 'e'
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, // 'f'
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // 'g'
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // 'h'
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // 'i'
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // 'j'
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // 'k'
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'l'
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // 'm'
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // 'n'
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // 'o'
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // 'p'
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // 'q'
    {0x00,0x00,0x3B,0x6E,0x06,0x06,0x0F,0x00}, // 'r'
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // 's'
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // 't'
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // 'u'
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 'v'
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // 'w'
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // 'x'
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // 'y'
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // 'z'
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // '{'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // '|'
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // '}'
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // '~'
};

// 字符集：只用一组视觉上单调、形状简单、等宽友好的字符，降低排序翻转风险
// 顺序按 8x8 模板实际墨覆盖率从暗到亮（后面会按实际覆盖率重排）
static const char* CAND_CHARS = " .:-=+*#%@|/\\_~<>^";
static const int NCAND_RAW = 20;

// ============================================================
// 3. 字形模板：每个字符 8x8 墨覆盖 -> 2x2 子格平均（供块内 4x4 子像素匹配）
//    这里保留 8x8 原始栅格，块内匹配时按 2x2 面积平均到 4x4 子像素
// ============================================================
struct Glyph {
    unsigned char pix[8][8]; // 0/1
    float ink;               // 整体墨覆盖率
    float sub2x2[4];         // 2x2 子格墨覆盖率（用于快速块匹配）
    float sub4x4[16];        // 4x4 子格墨覆盖率（字符占 2x2 子像素时用）
};

static Glyph make_glyph(char c) {
    Glyph g{};
    int idx = (int)c - 32;
    if (idx < 0 || idx >= 95) { std::memset(g.pix, 0, sizeof(g.pix)); }
    else {
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                g.pix[y][x] = (FONT8X8[idx][y] >> (7 - x)) & 1;
    }
    // 整体墨覆盖率
    int cnt = 0;
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) cnt += g.pix[y][x];
    g.ink = cnt / 64.0f;
    // 2x2 子格（每格 4x4 像素）
    for (int sy = 0; sy < 2; ++sy)
        for (int sx = 0; sx < 2; ++sx) {
            int s = 0;
            for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x)
                s += g.pix[sy*4+y][sx*4+x];
            g.sub2x2[sy*2+sx] = s / 16.0f;
        }
    // 4x4 子格（每格 2x2 像素）
    for (int sy = 0; sy < 4; ++sy)
        for (int sx = 0; sx < 4; ++sx) {
            int s = 0;
            for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x)
                s += g.pix[sy*2+y][sx*2+x];
            g.sub4x4[sy*4+sx] = s / 4.0f;
        }
    return g;
}

// ============================================================
// 4. 区域平均降采样（可分离，面积加权，支持逐步减半）
// ============================================================
static std::vector<float> area_downsample_gray(
    const std::vector<float>& gray, int w, int h, int gw, int gh)
{
    // 若比例很大，先逐步减半到接近目标，降低阻带不足
    std::vector<float> cur = gray;
    int cw = w, ch = h;
    while (cw >= gw*2 && ch >= gh*2 && cw > 2 && ch > 2) {
        int nw = cw/2, nh = ch/2;
        std::vector<float> nxt((size_t)nw*nh, 0.f);
        for (int y = 0; y < nh; ++y)
            for (int x = 0; x < nw; ++x) {
                float s = cur[(size_t)(2*y)*cw + 2*x]
                        + cur[(size_t)(2*y)*cw + 2*x+1]
                        + cur[(size_t)(2*y+1)*cw + 2*x]
                        + cur[(size_t)(2*y+1)*cw + 2*x+1];
                nxt[(size_t)y*nw + x] = s * 0.25f;
            }
        cur.swap(nxt); cw = nw; ch = nh;
    }
    // 最终面积加权
    std::vector<float> out((size_t)gw*gh, 0.f);
    std::vector<float> tmp((size_t)ch*gw, 0.f);
    for (int y = 0; y < ch; ++y)
        for (int gx = 0; gx < gw; ++gx) {
            float a = (float)gx * cw / gw, b = (float)(gx+1) * cw / gw;
            float sum = 0.f, wsum = 0.f;
            int i0 = (int)std::floor(a), i1 = (int)std::ceil(b);
            for (int i = std::max(0,i0); i < std::min(cw,i1); ++i) {
                float l = std::max(a, (float)i), r = std::min(b, (float)(i+1));
                float wgt = std::max(0.f, r - l);
                sum += cur[(size_t)y*cw + i] * wgt; wsum += wgt;
            }
            tmp[(size_t)y*gw + gx] = (wsum > 0) ? sum / wsum : 0.f;
        }
    for (int gy = 0; gy < gh; ++gy) {
        float a = (float)gy * ch / gh, b = (float)(gy+1) * ch / gh;
        for (int gx = 0; gx < gw; ++gx) {
            float sum = 0.f, wsum = 0.f;
            int j0 = (int)std::floor(a), j1 = (int)std::ceil(b);
            for (int j = std::max(0,j0); j < std::min(ch,j1); ++j) {
                float l = std::max(a, (float)j), r = std::min(b, (float)(j+1));
                float wgt = std::max(0.f, r - l);
                sum += tmp[(size_t)j*gw + gx] * wgt; wsum += wgt;
            }
            out[(size_t)gy*gw + gx] = (wsum > 0) ? sum / wsum : 0.f;
        }
    }
    return out;
}

// ============================================================
// 5. 主流程
// ============================================================
int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
            "usage: %s input.png cols rows [gain] [gamma] [--invert]\n"
            "  cols/rows = 字符块数（每块 2x2 字符，输出 2*cols x 2*rows 字符）\n"
            "  每块对应 4x4 子像素目标（每字符占 2x2 子像素）\n", argv[0]);
        return 1;
    }
    const char* path = argv[1];
    int cols = std::atoi(argv[2]);
    int rows = std::atoi(argv[3]);
    float gain  = (argc > 4 && argv[4][0] != '-') ? (float)std::atof(argv[4]) : 1.0f;
    float gamma = (argc > 5 && argv[5][0] != '-') ? (float)std::atof(argv[5]) : 1.0f;
    bool invert = false;
    for (int i = 4; i < argc; ++i) if (std::strcmp(argv[i], "--invert") == 0) invert = true;
    if (cols <= 0 || rows <= 0) { std::fprintf(stderr, "cols/rows must >0\n"); return 1; }

    int w, h, n;
    unsigned char* data = stbi_load(path, &w, &h, &n, 3);
    if (!data) { std::fprintf(stderr, "load failed: %s\n", stbi_failure_reason()); return 1; }

    // 转线性光灰度
    std::vector<float> lin((size_t)w*h);
    for (size_t i = 0; i < (size_t)w*h; ++i) {
        float r = srgb_to_linear(data[i*3+0]/255.0f);
        float g = srgb_to_linear(data[i*3+1]/255.0f);
        float b = srgb_to_linear(data[i*3+2]/255.0f);
        lin[i] = r*0.2126f + g*0.7152f + b*0.0722f;
    }
    stbi_image_free(data);

    // 子像素网格 = 4*cols x 4*rows（每块 4x4 子像素）
    int gw = cols*4, gh = rows*4;
    std::vector<float> target = area_downsample_gray(lin, w, h, gw, gh);

    // 亮度/gamma 调节（先调目标，再优化）
    for (auto& v : target) {
        v = std::clamp(v * gain, 0.0f, 1.0f);
        float s = linear_to_srgb(v);
        s = std::pow(s, gamma);
        v = srgb_to_linear(s);
    }
    if (invert) {
        // 深色背景：目标“视觉亮度”取反，字符墨覆盖率直接对应亮度
        for (auto& v : target) v = 1.0f - v;
    }

    // 构建候选字符模板，按 ink 从暗到亮排序（暗=墨多，对应目标亮度低）
    struct Cand { char c; Glyph g; };
    std::vector<Cand> cands;
    for (int i = 0; i < NCAND_RAW; ++i) {
        char c = CAND_CHARS[i];
        if (c == '\0') break;
        cands.push_back({c, make_glyph(c)});
    }
    std::sort(cands.begin(), cands.end(),
        [](const Cand& a, const Cand& b){ return a.g.ink > b.g.ink; }); // 墨多->暗
    int NC = (int)cands.size();

    // 预计算每个候选的 4x4 子格“视觉亮度” = 1 - 墨覆盖率
    std::vector<std::array<float,16>> vis4(NC);
    for (int i = 0; i < NC; ++i)
        for (int k = 0; k < 16; ++k)
            vis4[i][k] = 1.0f - cands[i].g.sub4x4[k];

    // 块内误差：字符占 2x2 子像素，块 2x2 字符 -> 4x4 子像素
    // 字符 (cx,cy) 在块内覆盖子像素 [cy*2..cy*2+1] x [cx*2..cx*2+1]
    auto block_err = [&](int bx, int by, const int idx[4]) -> float {
        float err = 0.f;
        for (int cy = 0; cy < 2; ++cy)
            for (int cx = 0; cx < 2; ++cx) {
                int ci = idx[cy*2+cx];
                for (int sy = 0; sy < 2; ++sy)
                    for (int sx = 0; sx < 2; ++sx) {
                        int gx = bx*4 + cx*2 + sx;
                        int gy = by*4 + cy*2 + sy;
                        float t = target[(size_t)gy*gw + gx];
                        float v = vis4[ci][(cy*2+sy)*4 + (cx*2+sx)];
                        float e = v - t;
                        err += e*e;
                    }
            }
        return err;
    };

    // 初始化：逐块贪心（对每个字符位置取独立最优，作为起点）
    std::vector<std::array<int,4>> choice((size_t)cols*rows);
    std::mt19937 rng(12345);
    for (int by = 0; by < rows; ++by)
        for (int bx = 0; bx < cols; ++bx) {
            // 多起点：随机若干 + 独立最优
            std::array<int,4> best{}; float bestE = std::numeric_limits<float>::max();
            int starts = 6;
            for (int s = 0; s < starts; ++s) {
                int idx[4];
                if (s == 0) {
                    // 独立最优：每个字符位置按该 2x2 子格平均选
                    for (int k = 0; k < 4; ++k) {
                        int cx = k % 2, cy = k / 2;
                        float tsum = 0.f;
                        for (int sy = 0; sy < 2; ++sy) for (int sx = 0; sx < 2; ++sx)
                            tsum += target[(size_t)(by*4+cy*2+sy)*gw + (bx*4+cx*2+sx)];
                        tsum *= 0.25f;
                        int bi = 0; float be = 1e9f;
                        for (int i = 0; i < NC; ++i) {
                            float v = 0.f;
                            for (int sy = 0; sy < 2; ++sy) for (int sx = 0; sx < 2; ++sx)
                                v += vis4[i][(cy*2+sy)*4 + (cx*2+sx)];
                            v *= 0.25f;
                            float e = (v-tsum)*(v-tsum);
                            if (e < be) { be = e; bi = i; }
                        }
                        idx[k] = bi;
                    }
                } else {
                    for (int k = 0; k < 4; ++k) idx[k] = rng() % NC;
                }
                float e = block_err(bx, by, idx);
                if (e < bestE) { bestE = e; best = {idx[0],idx[1],idx[2],idx[3]}; }
            }
            // 坐标下降精修
            for (int iter = 0; iter < 8; ++iter) {
                bool changed = false;
                for (int k = 0; k < 4; ++k) {
                    int curIdx[4] = {best[0],best[1],best[2],best[3]};
                    int bi = curIdx[k]; float be = block_err(bx, by, curIdx);
                    for (int i = 0; i < NC; ++i) {
                        if (i == curIdx[k]) continue;
                        curIdx[k] = i;
                        float e = block_err(bx, by, curIdx);
                        if (e < be) { be = e; bi = i; changed = true; }
                    }
                    curIdx[k] = bi; best[k] = bi;
                }
                if (!changed) break;
            }
            choice[(size_t)by*cols + bx] = best;
        }

    // 块间联合优化：若干轮，对每个块考虑与邻块交换/重选，降低全局能量
    // 全局能量 = 各块块内误差 + 轻量边界平滑项（相邻块对应子像素差）
    auto neighbor_smooth = [&](int bx, int by, const int idx[4]) -> float {
        float e = 0.f;
        // 右边界：本块右列 vs 右邻块左列（若存在）
        if (bx+1 < cols) {
            const auto& nb = choice[(size_t)by*cols + (bx+1)];
            for (int cy = 0; cy < 2; ++cy)
                for (int sy = 0; sy < 2; ++sy) {
                    float a = vis4[idx[cy*2+1]][(cy*2+sy)*4 + 3];
                    float b = vis4[nb[cy*2+0]][(cy*2+sy)*4 + 0];
                    e += (a-b)*(a-b);
                }
        }
        // 下边界
        if (by+1 < rows) {
            const auto& nb = choice[(size_t)(by+1)*cols + bx];
            for (int cx = 0; cx < 2; ++cx)
                for (int sx = 0; sx < 2; ++sx) {
                    float a = vis4[idx[1*2+cx]][(1*2+0)*4 + (cx*2+sx)];
                    float b = vis4[nb[0*2+cx]][(0*2+1)*4 + (cx*2+sx)];
                    e += (a-b)*(a-b);
                }
        }
        return e;
    };

    const float LAMBDA = 0.15f;
    for (int pass = 0; pass < 4; ++pass) {
        bool any = false;
        for (int by = 0; by < rows; ++by)
            for (int bx = 0; bx < cols; ++bx) {
                auto& cur = choice[(size_t)by*cols + bx];
                float curE = block_err(bx, by, cur.data()) + LAMBDA * neighbor_smooth(bx, by, cur.data());
                std::array<int,4> best = cur; float bestE = curE;
                // 候选：坐标下降 + 少量随机扰动
                for (int trial = 0; trial < 3; ++trial) {
                    std::array<int,4> cand = cur;
                    if (trial == 0) {
                        for (int k = 0; k < 4; ++k) cand[k] = rng() % NC;
                    } else {
                        for (int k = 0; k < 4; ++k) if (rng() % 2) cand[k] = rng() % NC;
                    }
                    for (int iter = 0; iter < 4; ++iter) {
                        bool ch = false;
                        for (int k = 0; k < 4; ++k) {
                            int bi = cand[k]; float be = block_err(bx,by,cand.data()) + LAMBDA*neighbor_smooth(bx,by,cand.data());
                            for (int i = 0; i < NC; ++i) {
                                if (i == cand[k]) continue;
                                cand[k] = i;
                                float e = block_err(bx,by,cand.data()) + LAMBDA*neighbor_smooth(bx,by,cand.data());
                                if (e < be) { be = e; bi = i; ch = true; }
                            }
                            cand[k] = bi;
                        }
                        if (!ch) break;
                    }
                    float e = block_err(bx,by,cand.data()) + LAMBDA*neighbor_smooth(bx,by,cand.data());
                    if (e < bestE) { bestE = e; best = cand; }
                }
                if (best != cur) { cur = best; any = true; }
            }
        if (!any) break;
    }

    // 输出字符网格 2*rows x 2*cols
    std::vector<std::string> out(rows*2, std::string(cols*2, ' '));
    for (int by = 0; by < rows; ++by)
        for (int bx = 0; bx < cols; ++bx) {
            const auto& idx = choice[(size_t)by*cols + bx];
            out[by*2+0][bx*2+0] = cands[idx[0]].c;
            out[by*2+0][bx*2+1] = cands[idx[1]].c;
            out[by*2+1][bx*2+0] = cands[idx[2]].c;
            out[by*2+1][bx*2+1] = cands[idx[3]].c;
        }
    for (auto& s : out) std::puts(s.c_str());
    return 0;
}