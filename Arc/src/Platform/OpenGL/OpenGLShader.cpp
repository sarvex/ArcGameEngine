#include "arcpch.h"
#include "Platform/OpenGL/OpenGLShader.h"

#include <fstream>
#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>

namespace ArcEngine
{
	static GLenum ShaderTypeFromString(const std::string& type)
	{
		if(type == "vertex")
			return GL_VERTEX_SHADER;
		if(type == "fragment" || type == "pixel")
			return GL_FRAGMENT_SHADER;
		if(type == "compute")
			return GL_COMPUTE_SHADER;

		ARC_CORE_ASSERT(false, "Unknown shader type!");
		return 0;
	}
	
	OpenGLShader::OpenGLShader(const std::string& filepath)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		std::string source = ReadFile(filepath);
		auto shaderSources = PreProcess(source);
		Compile(shaderSources);
		
		// Extract name from filepath
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		m_Name = filepath.substr(lastSlash, count);
	}

	OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
		: m_Name(name)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		std::unordered_map<GLenum, std::string> sources;
		sources[GL_VERTEX_SHADER] = vertexSrc;
		sources[GL_FRAGMENT_SHADER] = fragmentSrc;
		Compile(sources);
	}

	OpenGLShader::~OpenGLShader()
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		glDeleteProgram(m_RendererID);
	}

	void OpenGLShader::Recompile(const std::string& filepath)
	{
		OPTICK_EVENT();

		glDeleteProgram(m_RendererID);

		std::string source = ReadFile(filepath);
		auto shaderSources = PreProcess(source);
		Compile(shaderSources);
	}

	void OpenGLShader::Bind() const
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		glUseProgram(m_RendererID);
	}

	void OpenGLShader::Unbind() const
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		glUseProgram(0);
	}

	void OpenGLShader::SetInt(const std::string& name, int value)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		UploadUniformInt(name, value);
	}

	void OpenGLShader::SetIntArray(const std::string& name, int* values, uint32_t count)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		UploadUniformIntArray(name, values, count);		
	}

	void OpenGLShader::SetFloat(const std::string& name, float value)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		UploadUniformFloat(name, value);
	}

	void OpenGLShader::SetFloat2(const std::string& name, const glm::vec2& value)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		UploadUniformFloat2(name, value);
	}

	void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		UploadUniformFloat3(name, value);
	}

	void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		UploadUniformFloat4(name, value);
	}

	void OpenGLShader::SetMat3(const std::string& name, const glm::mat3& value)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		UploadUniformMat3(name, value);
	}

	void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& value)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		UploadUniformMat4(name, value);
	}

	void OpenGLShader::SetUniformBlock(const std::string& name, uint32_t blockIndex)
	{
		glUniformBlockBinding(m_RendererID, glGetUniformBlockIndex(m_RendererID, name.c_str()), blockIndex);
	}

	void OpenGLShader::UploadUniformInt(const std::string& name, int value)
	{
		glUniform1i(GetLocation(name), value);
	}

	void OpenGLShader::UploadUniformIntArray(const std::string& name, int* values, uint32_t count)
	{
		glUniform1iv(GetLocation(name), count, values);
	}

	void OpenGLShader::UploadUniformFloat(const std::string& name, float value)
	{
		glUniform1f(GetLocation(name), value);
	}

	void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& values)
	{
		glUniform2f(GetLocation(name), values.x, values.y);
	}

	void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& values)
	{
		glUniform3f(GetLocation(name), values.x, values.y, values.z);
	}

	void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& values)
	{
		glUniform4f(GetLocation(name), values.x, values.y, values.z, values.w);
	}
	
	void OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix)
	{
		glUniformMatrix3fv(GetLocation(name), 1, GL_FALSE, glm::value_ptr(matrix));
	}

	void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
	{
		glUniformMatrix4fv(GetLocation(name), 1, GL_FALSE, glm::value_ptr(matrix));
	}

	int OpenGLShader::GetLocation(const std::string& name)
	{
		OPTICK_EVENT();

		if (m_UniformLocationCache.find(name) != m_UniformLocationCache.end())
			return m_UniformLocationCache.at(name);

		int location = glGetUniformLocation(m_RendererID, name.c_str());
		m_UniformLocationCache.emplace(name, location);
		return location;
	}

	std::string OpenGLShader::ReadFile(const std::string& filepath)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		std::string result;
		std::ifstream in(filepath, std::ios::in | std::ios::binary); // ifstream closes itself due to RAII
		if (in)
		{
			in.seekg(0, std::ios::end);
			const size_t size = in.tellg();
			if (size != -1)
			{
				result.resize(size);
				in.seekg(0, std::ios::beg);
				in.read(&result[0], size);
			}
			else
			{
				ARC_CORE_ERROR("Could not read from file '{0}'", filepath);
			}
		}
		else
		{
			ARC_CORE_ERROR("Could not open file '{0}'", filepath);
		}

		return result;
	}

	std::unordered_map<GLenum, std::string> OpenGLShader::PreProcess(const std::string& source)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		std::unordered_map<GLenum, std::string> shaderSources;

		const char* typeToken = "#type";
		size_t typeTokenLength = strlen(typeToken);
		size_t pos = source.find(typeToken, 0);
		while (pos != std::string::npos)
		{
			size_t eol = source.find_first_of("\r\n", pos);
			ARC_CORE_ASSERT(eol != std::string::npos, "Syntax error");
			size_t begin = pos + typeTokenLength + 1;
			std::string type = source.substr(begin, eol - begin);
			ARC_CORE_ASSERT(ShaderTypeFromString(type), "Invalid shader type specified");

			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			pos = source.find(typeToken, nextLinePos);
			shaderSources[ShaderTypeFromString(type)] = source.substr(nextLinePos, pos - (nextLinePos == std::string::npos ? source.size() - 1 : nextLinePos));
		}

		return shaderSources;
	}

	static MaterialPropertyType GetMaterialPropertyType(GLenum property)
	{
		switch (property)
		{
			case GL_SAMPLER_2D:		return MaterialPropertyType::Sampler2D;
			case GL_BOOL:			return MaterialPropertyType::Bool;
			case GL_INT:			return MaterialPropertyType::Int;
			case GL_FLOAT:			return MaterialPropertyType::Float;
			case GL_FLOAT_VEC2:		return MaterialPropertyType::Float2;
			case GL_FLOAT_VEC3:		return MaterialPropertyType::Float3;
			case GL_FLOAT_VEC4:		return MaterialPropertyType::Float4;
		}

		return MaterialPropertyType::None;
	}

	void OpenGLShader::Compile(const std::unordered_map<GLenum, std::string>& shaderSources)
	{
		OPTICK_EVENT();

		ARC_PROFILE_FUNCTION();
		
		GLuint program = glCreateProgram();
		ARC_CORE_ASSERT(shaderSources.size() <= 2, "We only support 2 shaders for now");
		std::array<GLenum, 2> glShaderIDs;
		int glShaderIDIndex = 0;
		for (auto kv : shaderSources)
		{
			GLenum type = kv.first;
			const std::string& source = kv.second;

			GLuint shader = glCreateShader(type);
			
			const GLchar* sourceCStr = source.c_str();
			glShaderSource(shader, 1, &sourceCStr, 0);

			// Compile the vertex shader
			glCompileShader(shader);

			GLint isCompiled = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
			if(isCompiled == GL_FALSE)
			{
				GLint maxLength = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

				std::vector<GLchar> infoLog(maxLength);
				glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);
				
				glDeleteShader(shader);

				ARC_CORE_ERROR("{0}", infoLog.data());
				ARC_CORE_ASSERT(false, "Shader compilation failure!");
				break;
			}

			glAttachShader(program, shader);
			glShaderIDs[glShaderIDIndex++] = shader;
		}

		glLinkProgram(program);

		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, (int *)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
			
			glDeleteProgram(program);
			for (auto id : glShaderIDs)
				glDeleteShader(id);

			ARC_CORE_ERROR("{0}", infoLog.data());
			ARC_CORE_ASSERT(false, "Shader link failure!");
			return;
		}

		for (auto id : glShaderIDs)
			glDetachShader(program, id);

		m_RendererID = program;

		// Get material properties from the shader
		m_MaterialProperties.clear();
		int maxLength;
		glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);

		int uniformCount;
		glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniformCount);

		size_t offset = 0;
		for (size_t i = 0; i < uniformCount; i++)
		{
			char name[128];
			int size;
			GLenum type;
			glGetActiveUniform(program, i, maxLength, nullptr, &size, &type, &name[0]);

			static const char* prefix = "u_Material.";
			if (strncmp(name, prefix, strlen(prefix)) == 0)
			{
				MaterialPropertyType propertyType = GetMaterialPropertyType(type);
				size_t sizeInBytes = GetSizeInBytes(propertyType);
				m_MaterialProperties.emplace(name, MaterialProperty{ propertyType, sizeInBytes, offset });
				offset += sizeInBytes;
			}
		}
	}
}
