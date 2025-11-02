#pragma once

#include "gl_common.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <glm/glm.hpp>


class Shader {
    public:
        Shader() = default;
        Shader(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);

        void use() const;

        void setBool(const std::string &name, bool value) const;
        void setInt(const std::string &name, int value) const;
        void setFloat(const std::string &name, float value) const;
        void setFloat2(const std::string &name, const glm::vec2 &value) const;
        void setFloat3(const std::string &name, const glm::vec3 &value) const;
        void setMat4(const std::string &name, const glm::mat4 &mat) const;

        unsigned int shader_id;

        operator bool() const { return shader_id != 0; }


    // Use glUniform* functions to set uniform variables in the shader
    // private:
    //     void UploadBoolean(const std::string &name, bool value) const;
    //     void UploadInt(const std::string &name, int value) const;
    //     void UploadFloat(const std::string &name, float value) const;
    //     void UploadFloat2(const std::string &name, const glm::vec2 &value) const;
    //     void UploadFloat3(const std::string &name, const glm::vec3 &value) const;
    //     void UploadMat4(const std::string &name, const glm::mat4 &mat) const;

    private:
};        

