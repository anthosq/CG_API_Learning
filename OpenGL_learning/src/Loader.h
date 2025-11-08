#pragma once

#include "gl_common.h"
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>

// DDS 文件格式常量
#define DDS_MAGIC 0x20534444 // "DDS "

// DDS Header Flags
#define DDSD_CAPS 0x1
#define DDSD_HEIGHT 0x2
#define DDSD_WIDTH 0x4
#define DDSD_PITCH 0x8
#define DDSD_PIXELFORMAT 0x1000
#define DDSD_MIPMAPCOUNT 0x20000
#define DDSD_LINEARSIZE 0x80000
#define DDSD_DEPTH 0x800000

// DDS Pixel Format Flags
#define DDPF_ALPHAPIXELS 0x1
#define DDPF_ALPHA 0x2
#define DDPF_FOURCC 0x4
#define DDPF_RGB 0x40
#define DDPF_YUV 0x200
#define DDPF_LUMINANCE 0x20000

// DDS Caps Flags
#define DDSCAPS_COMPLEX 0x8
#define DDSCAPS_MIPMAP 0x400000
#define DDSCAPS_TEXTURE 0x1000

// FourCC Codes
#define FOURCC_DXT1 0x31545844 // "DXT1"
#define FOURCC_DXT2 0x32545844 // "DXT2"
#define FOURCC_DXT3 0x33545844 // "DXT3"
#define FOURCC_DXT4 0x34545844 // "DXT4"
#define FOURCC_DXT5 0x35545844 // "DXT5"
#define FOURCC_DX10 0x30315844 // "DX10"

// BC 压缩格式（DirectX 10+）
#define FOURCC_BC1  FOURCC_DXT1
#define FOURCC_BC2  FOURCC_DXT3
#define FOURCC_BC3  FOURCC_DXT5
#define FOURCC_BC4U 0x55344342 // "BC4U"
#define FOURCC_BC4S 0x53344342 // "BC4S"
#define FOURCC_BC5U 0x55354342 // "BC5U"
#define FOURCC_BC5S 0x53354342 // "BC5S"

namespace DDSLoader {

    // DDS 像素格式结构
    struct DDSPixelFormat {
        uint32_t size;
        uint32_t flags;
        uint32_t fourCC;
        uint32_t rgbBitCount;
        uint32_t rBitMask;
        uint32_t gBitMask;
        uint32_t bBitMask;
        uint32_t aBitMask;
    };

    // DDS 头部结构
    struct DDSHeader {
        uint32_t size;
        uint32_t flags;
        uint32_t height;
        uint32_t width;
        uint32_t pitchOrLinearSize;
        uint32_t depth;
        uint32_t mipMapCount;
        uint32_t reserved1[11];
        DDSPixelFormat ddspf;
        uint32_t caps;
        uint32_t caps2;
        uint32_t caps3;
        uint32_t caps4;
        uint32_t reserved2;
    };

    // DX10 扩展头部
    struct DDSHeaderDX10 {
        uint32_t dxgiFormat;
        uint32_t resourceDimension;
        uint32_t miscFlag;
        uint32_t arraySize;
        uint32_t miscFlags2;
    };

    // DDS 纹理信息
    struct DDSTextureInfo {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t mipMapCount;
        uint32_t arraySize;
        GLenum format;           // OpenGL 内部格式
        GLenum target;           // OpenGL 纹理目标
        bool isCompressed;
        uint32_t blockSize;      // 压缩格式的块大小
        std::string formatName;  // 格式名称（用于调试）
    };

    // 将 FourCC 转换为 OpenGL 格式
    inline bool GetFormatFromFourCC(uint32_t fourCC, DDSTextureInfo& info) {
        info.isCompressed = true;
        
        switch (fourCC) {
            case FOURCC_DXT1:
                info.format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                info.blockSize = 8;
                info.formatName = "DXT1/BC1";
                return true;
                
            case FOURCC_DXT3:
                info.format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                info.blockSize = 16;
                info.formatName = "DXT3/BC2";
                return true;
                
            case FOURCC_DXT5:
                info.format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                info.blockSize = 16;
                info.formatName = "DXT5/BC3";
                return true;
                
            default:
                std::cerr << "Unsupported DDS FourCC: 0x" << std::hex << fourCC << std::dec << std::endl;
                return false;
        }
    }

    // 从像素格式获取 OpenGL 格式（未压缩）
    inline bool GetFormatFromPixelFormat(const DDSPixelFormat& ddspf, DDSTextureInfo& info) {
        info.isCompressed = false;
        info.blockSize = 0;

        if (ddspf.flags & DDPF_RGB) {
            if (ddspf.flags & DDPF_ALPHAPIXELS) {
                // RGBA 格式
                if (ddspf.rgbBitCount == 32) {
                    info.format = GL_RGBA8;
                    info.formatName = "RGBA8";
                    return true;
                }
            } else {
                // RGB 格式
                if (ddspf.rgbBitCount == 24) {
                    info.format = GL_RGB8;
                    info.formatName = "RGB8";
                    return true;
                }
            }
        }
        
        if (ddspf.flags & DDPF_LUMINANCE) {
            if (ddspf.rgbBitCount == 8) {
                info.format = GL_R8;
                info.formatName = "L8";
                return true;
            }
        }

        std::cerr << "Unsupported uncompressed DDS format" << std::endl;
        return false;
    }

    // 计算 mipmap 级别的数据大小
    inline uint32_t CalculateMipSize(uint32_t width, uint32_t height, uint32_t blockSize, bool isCompressed) {
        if (isCompressed) {
            return ((width + 3) / 4) * ((height + 3) / 4) * blockSize;
        } else {
            // 假设 32 位 RGBA
            return width * height * 4;
        }
    }

    // 加载 DDS 文件
    inline GLuint LoadDDS(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open DDS file: " << filename << std::endl;
            return 0;
        }

        // 读取 Magic Number
        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), sizeof(uint32_t));
        
        if (magic != DDS_MAGIC) {
            std::cerr << "Invalid DDS file (bad magic number): " << filename << std::endl;
            file.close();
            return 0;
        }

        // 读取主头部
        DDSHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(DDSHeader));

        if (header.size != 124) {
            std::cerr << "Invalid DDS header size: " << header.size << std::endl;
            file.close();
            return 0;
        }

        // 解析纹理信息
        DDSTextureInfo info;
        info.width = header.width;
        info.height = header.height;
        info.depth = (header.flags & DDSD_DEPTH) ? header.depth : 1;
        info.mipMapCount = (header.flags & DDSD_MIPMAPCOUNT) ? header.mipMapCount : 1;
        info.arraySize = 1;
        info.target = GL_TEXTURE_2D;

        // 检查是否有 DX10 扩展头
        if (header.ddspf.fourCC == FOURCC_DX10) {
            DDSHeaderDX10 dx10Header;
            file.read(reinterpret_cast<char*>(&dx10Header), sizeof(DDSHeaderDX10));
            
            std::cerr << "DX10 extended header not fully supported yet" << std::endl;
            file.close();
            return 0;
        }

        // 确定格式
        bool formatOk = false;
        if (header.ddspf.flags & DDPF_FOURCC) {
            formatOk = GetFormatFromFourCC(header.ddspf.fourCC, info);
        } else {
            formatOk = GetFormatFromPixelFormat(header.ddspf, info);
        }

        if (!formatOk) {
            std::cerr << "Unsupported DDS format in file: " << filename << std::endl;
            file.close();
            return 0;
        }

        std::cout << "Loading DDS: " << filename << std::endl;
        std::cout << "  Format: " << info.formatName << std::endl;
        std::cout << "  Size: " << info.width << "x" << info.height << std::endl;
        std::cout << "  Mipmaps: " << info.mipMapCount << std::endl;
        std::cout << "  Compressed: " << (info.isCompressed ? "Yes" : "No") << std::endl;

        // 创建 OpenGL 纹理
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(info.target, textureID);

        // 读取并上传每个 mipmap 级别
        uint32_t w = info.width;
        uint32_t h = info.height;

        for (uint32_t level = 0; level < info.mipMapCount; ++level) {
            // 计算当前级别的数据大小
            uint32_t size = CalculateMipSize(w, h, info.blockSize, info.isCompressed);
            
            // 读取数据
            std::vector<uint8_t> data(size);
            file.read(reinterpret_cast<char*>(data.data()), size);
            
            if (!file || file.gcount() != size) {
                std::cerr << "Failed to read mipmap level " << level 
                         << " (expected " << size << " bytes, got " << file.gcount() << ")" << std::endl;
                glDeleteTextures(1, &textureID);
                file.close();
                return 0;
            }

            // 上传到 GPU
            if (info.isCompressed) {
                glCompressedTexImage2D(
                    info.target,
                    level,
                    info.format,
                    w, h,
                    0,
                    size,
                    data.data()
                );
            } else {
                // 未压缩格式
                GLenum format = GL_RGBA;
                GLenum type = GL_UNSIGNED_BYTE;
                
                if (info.format == GL_RGB8) {
                    format = GL_RGB;
                } else if (info.format == GL_R8) {
                    format = GL_RED;
                }
                
                glTexImage2D(
                    info.target,
                    level,
                    info.format,
                    w, h,
                    0,
                    format,
                    type,
                    data.data()
                );
            }

            // 检查 OpenGL 错误
            GLenum error = glGetError();
            if (error != GL_NO_ERROR) {
                std::cerr << "OpenGL error while uploading mipmap " << level 
                         << ": 0x" << std::hex << error << std::dec << std::endl;
            }

            // 下一个 mipmap 级别
            w = std::max(1u, w / 2);
            h = std::max(1u, h / 2);
        }

        file.close();

        // 设置纹理参数
        glTexParameteri(info.target, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(info.target, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        if (info.mipMapCount > 1) {
            glTexParameteri(info.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(info.target, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(info.target, GL_TEXTURE_MAX_LEVEL, info.mipMapCount - 1);
        } else {
            glTexParameteri(info.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
        glTexParameteri(info.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        std::cout << "Successfully loaded DDS texture: " << filename << std::endl;
        
        return textureID;
    }

    // 检查系统是否支持 DDS 压缩格式
    inline bool CheckDDSSupport() {
        // 检查 S3TC 扩展
        const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        
        bool s3tcSupported = false;
        if (extensions) {
            std::string extStr(extensions);
            s3tcSupported = extStr.find("GL_EXT_texture_compression_s3tc") != std::string::npos;
        }

        if (s3tcSupported) {
            std::cout << "✓ S3TC texture compression supported (DXT1/3/5)" << std::endl;
        } else {
            std::cerr << "✗ WARNING: S3TC texture compression not supported!" << std::endl;
        }

        return s3tcSupported;
    }

    // 创建默认纹理（用于加载失败时）
    inline GLuint CreateDefaultTexture() {
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        // 创建 8x8 棋盘纹理（粉色和黑色）
        const int size = 8;
        unsigned char data[size * size * 3];
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                int index = (i * size + j) * 3;
                bool isPink = ((i / 2) + (j / 2)) % 2 == 0;
                data[index + 0] = isPink ? 255 : 0;   // R
                data[index + 1] = isPink ? 0 : 0;     // G
                data[index + 2] = isPink ? 255 : 0;   // B
            }
        }
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        return textureID;
    }

} // namespace DDSLoader