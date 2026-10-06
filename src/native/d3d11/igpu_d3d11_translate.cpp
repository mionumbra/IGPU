#include "igpu_d3d11.h"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include <spirv_hlsl.hpp>

#include <mutex>
#include <string>
#include <vector>

#include "../igpu_error.h"

namespace igpu
{
namespace d3d11_impl
{
    namespace
    {
        bool glsl_language(std::int32_t stage, EShLanguage& out)
        {
            switch (stage)
            {
            case 0: out = EShLangVertex; return true;
            case 1: out = EShLangFragment; return true;
            case 2: out = EShLangCompute; return true;
            case 3: out = EShLangGeometry; return true;
            case 4: out = EShLangTessControl; return true;
            case 5: out = EShLangTessEvaluation; return true;
            default: return false;
            }
        }

        void ensure_glslang()
        {
            static std::once_flag once;
            std::call_once(once, [] { glslang::InitializeProcess(); });
        }
    }

    bool translate_glsl_to_hlsl(std::string_view source, std::int32_t stage, std::string_view entry,
                                std::string& hlsl)
    {
        EShLanguage language{};
        if (!glsl_language(stage, language))
        {
            set_last_error("igpu_shader_compile: this stage cannot be translated from glsl");
            return false;
        }

        try
        {
            ensure_glslang();

            const std::string source_text(source);
            const std::string entry_name(entry);
            const char* sources[] = { source_text.c_str() };

            glslang::TShader shader(language);
            shader.setStrings(sources, 1);
            shader.setEntryPoint(entry_name.c_str());
            shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 100);
            shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
            shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

            const EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
            const TBuiltInResource* resources = GetDefaultResources();
            if (resources == nullptr || !shader.parse(resources, 100, false, messages))
            {
                std::string message = "igpu_shader_compile: the glsl shader could not be translated";
                const char* info = shader.getInfoLog();
                if (info != nullptr && info[0] != '\0')
                {
                    message += ": ";
                    message += info;
                }
                set_last_error(std::move(message));
                return false;
            }

            glslang::TProgram program;
            program.addShader(&shader);
            if (!program.link(messages) || program.getIntermediate(language) == nullptr)
            {
                std::string message = "igpu_shader_compile: the glsl shader could not be translated";
                const char* info = program.getInfoLog();
                if (info != nullptr && info[0] != '\0')
                {
                    message += ": ";
                    message += info;
                }
                set_last_error(std::move(message));
                return false;
            }

            std::vector<unsigned int> spirv;
            glslang::SpvOptions options;
            glslang::GlslangToSpv(*program.getIntermediate(language), spirv, &options);
            if (spirv.empty())
            {
                set_last_error("igpu_shader_compile: the glsl shader could not be translated");
                return false;
            }

            std::vector<uint32_t> words(spirv.begin(), spirv.end());
            spirv_cross::CompilerHLSL compiler(std::move(words));
            auto hlsl_options = compiler.get_hlsl_options();
            hlsl_options.shader_model = 50;
            compiler.set_hlsl_options(hlsl_options);
            hlsl = compiler.compile();
            if (hlsl.empty())
            {
                set_last_error("igpu_shader_compile: the glsl shader could not be translated");
                return false;
            }
            return true;
        }
        catch (const std::exception& ex)
        {
            set_last_error(std::string("igpu_shader_compile: the glsl shader could not be translated: ") + ex.what());
            return false;
        }
    }
}
}
