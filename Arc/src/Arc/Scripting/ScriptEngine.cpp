#include "arcpch.h"
#include "ScriptEngine.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/threads.h>
#include <mono/metadata/mono-debug.h>

#ifdef ARC_DEBUG
#include <mono/metadata/debug-helpers.h>
#endif // ARC_DEBUG

#include "Arc/Project/Project.h"
#include "Arc/Scene/Entity.h"
#include "Arc/Scene/Scene.h"
#include "MonoUtils.h"
#include "GCManager.h"
#include "ProjectBuilder.h"
#include "ScriptEngineRegistry.h"

namespace ArcEngine
{
	static const std::unordered_map<std::string, FieldType, UM_StringTransparentEquality> s_ScriptFieldTypeMap =
	{
		{ "System.Single",		FieldType::Float },
		{ "System.Double",		FieldType::Double },
		{ "System.Boolean",		FieldType::Bool },
		{ "System.Char",		FieldType::Char },
		{ "System.SByte",		FieldType::Byte },
		{ "System.Int16",		FieldType::Short },
		{ "System.Int32",		FieldType::Int },
		{ "System.Int64",		FieldType::Long },
		{ "System.Byte",		FieldType::UByte },
		{ "System.UInt16",		FieldType::UShort },
		{ "System.UInt32",		FieldType::UInt },
		{ "System.UInt64",		FieldType::ULong },
		{ "System.String",		FieldType::String },
		
		{ "ArcEngine.Vector2",	FieldType::Vector2 },
		{ "ArcEngine.Vector3",	FieldType::Vector3 },
		{ "ArcEngine.Vector4",	FieldType::Vector4 },
		{ "ArcEngine.Color",	FieldType::Color },
	};

	struct ScriptEngineData
	{
		MonoDomain* RootDomain = nullptr;
		MonoDomain* AppDomain = nullptr;

		MonoAssembly* CoreAssembly = nullptr;
		MonoAssembly* AppAssembly = nullptr;
		MonoImage* CoreImage = nullptr;
		MonoImage* AppImage = nullptr;

		MonoClass* EntityClass = nullptr;
		MonoClass* SerializeFieldAttribute = nullptr;
		MonoClass* HideInPropertiesAttribute = nullptr;
		MonoClass* ShowInPropertiesAttribute = nullptr;
		MonoClass* HeaderAttribute = nullptr;
		MonoClass* TooltipAttribute = nullptr;
		MonoClass* RangeAttribute = nullptr;

		bool EnableDebugging = true;

		std::unordered_map<std::string, Ref<ScriptClass>, UM_StringTransparentEquality> EntityClasses;
		std::unordered_map<UUID, std::unordered_map<std::string, std::unordered_map<std::string, ScriptFieldInstance, UM_StringTransparentEquality>, UM_StringTransparentEquality>> EntityFields;

		using EntityInstanceMap = std::unordered_map<UUID, std::unordered_map<std::string, ScriptInstance*, UM_StringTransparentEquality>>;
		EntityInstanceMap EntityRuntimeInstances;
	};

	static Scope<ScriptEngineData> s_Data;

	Scene* ScriptEngine::s_CurrentScene = nullptr;

	void ScriptEngine::Init()
	{
		ARC_PROFILE_SCOPE()

#if defined(ARC_PLATFORM_WINDOWS)
		mono_set_assemblies_path("mono/Win64/lib");
#elif defined(ARC_PLATFORM_LINUX)
		mono_set_assemblies_path("mono/Linux/lib");
#endif

		s_Data = CreateScope<ScriptEngineData>();

		if (s_Data->EnableDebugging)
		{
			static char* options[] =
			{
				const_cast<char*>(
					"--debugger-agent=transport=dt_socket,address=127.0.0.1:2550,server=y,suspend=n,loglevel=3,logfile=MonoDebugger.log"),
				const_cast<char*>("--soft-breakpoints")
			};
			mono_jit_parse_options(2, options);
			mono_debug_init(MONO_DEBUG_FORMAT_MONO);
		}

		s_Data->RootDomain = mono_jit_init("ArcJITRuntime");
		mono_domain_set(s_Data->RootDomain, false);

		if (s_Data->EnableDebugging)
			mono_debug_domain_create(s_Data->RootDomain);
		
		mono_thread_set_main(mono_thread_current());

		GCManager::Init();
		ScriptEngineRegistry::RegisterInternalCalls();

		ReloadAppDomain();
	}

	void ScriptEngine::Shutdown()
	{
		ARC_PROFILE_SCOPE()

		s_Data->EntityClasses.clear();
		s_Data->EntityFields.clear();
		s_Data->EntityRuntimeInstances.clear();

		mono_domain_set(s_Data->RootDomain, false);
		if (s_Data->AppDomain)
			mono_domain_unload(s_Data->AppDomain);

		GCManager::CollectGarbage();
		GCManager::Shutdown();

		mono_jit_cleanup(s_Data->RootDomain);

		s_Data->AppDomain = nullptr;
		s_Data->RootDomain = nullptr;
	}

	void ScriptEngine::LoadCoreAssembly()
	{
		ARC_PROFILE_SCOPE()

		if (!Project::GetActive())
			return;

		const std::filesystem::path path = "Resources/Scripts/Arc-ScriptCore.dll";
		s_Data->CoreAssembly = MonoUtils::LoadMonoAssembly(path, s_Data->EnableDebugging);
		s_Data->CoreImage = mono_assembly_get_image(s_Data->CoreAssembly);

		s_Data->EntityClass = mono_class_from_name(s_Data->CoreImage, "ArcEngine", "Entity");
		s_Data->SerializeFieldAttribute = mono_class_from_name(s_Data->CoreImage, "ArcEngine", "SerializeFieldAttribute");
		s_Data->HideInPropertiesAttribute = mono_class_from_name(s_Data->CoreImage, "ArcEngine", "HideInPropertiesAttribute");
		s_Data->ShowInPropertiesAttribute = mono_class_from_name(s_Data->CoreImage, "ArcEngine", "ShowInPropertiesAttribute");
		s_Data->HeaderAttribute = mono_class_from_name(s_Data->CoreImage, "ArcEngine", "HeaderAttribute");
		s_Data->TooltipAttribute = mono_class_from_name(s_Data->CoreImage, "ArcEngine", "TooltipAttribute");
		s_Data->RangeAttribute = mono_class_from_name(s_Data->CoreImage, "ArcEngine", "RangeAttribute");
	}

	void ScriptEngine::LoadClientAssembly()
	{
		ARC_PROFILE_SCOPE()
		
		const auto project = Project::GetActive();
		if (!project)
			return;

		const std::string clientAssemblyName = project->GetConfig().Name + ".dll";
		const auto path = Project::GetScriptModuleDirectory() / clientAssemblyName;
		if (std::filesystem::exists(path))
		{
			s_Data->AppAssembly = MonoUtils::LoadMonoAssembly(path, s_Data->EnableDebugging);
			s_Data->AppImage = mono_assembly_get_image(s_Data->AppAssembly);

			s_Data->EntityClasses.clear();
			LoadAssemblyClasses(s_Data->AppAssembly);

			ScriptEngineRegistry::ClearTypes();
			ScriptEngineRegistry::RegisterTypes();
		}
	}

	void ScriptEngine::ReloadAppDomain()
	{
		ARC_PROFILE_SCOPE()

		if (!Project::GetActive())
			return;

		const auto begin = std::chrono::high_resolution_clock::now();

		if (!ProjectBuilder::GenerateProjectFiles())
			return;

		const bool buildSuccess = ProjectBuilder::BuildProject();

		const auto end = std::chrono::high_resolution_clock::now();
		const auto timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

		if (!buildSuccess)
		{
			ARC_CORE_ERROR("Build Failed; Compile time: {0}ms", timeTaken);
			return;
		}

		ARC_CORE_INFO("Build Suceeded; Compile time: {0}ms", timeTaken);

		if (s_Data->AppDomain)
		{
			mono_domain_set(s_Data->RootDomain, false);
			mono_domain_unload(s_Data->AppDomain);
			s_Data->AppDomain = nullptr;
		}

		s_Data->AppDomain = mono_domain_create_appdomain(const_cast<char*>("ScriptRuntime"), nullptr);
		ARC_CORE_ASSERT(s_Data->AppDomain)
		mono_domain_set(s_Data->AppDomain, true);

		LoadCoreAssembly();
		LoadClientAssembly();

		GCManager::CollectGarbage();
	}

	void ScriptEngine::LoadAssemblyClasses(MonoAssembly* assembly)
	{
		ARC_PROFILE_SCOPE()

		MonoImage* image = mono_assembly_get_image(assembly);
		const MonoTableInfo* typeDefinitionTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
		const int32_t numTypes = mono_table_info_get_rows(typeDefinitionTable);

		for (int32_t i = 0; i < numTypes; ++i)
		{
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionTable, i, cols, MONO_TYPEDEF_SIZE);

			const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);
			std::string fullname = fmt::format("{}.{}", nameSpace, name);

			MonoClass* monoClass = mono_class_from_name(image, nameSpace, name);
			if (!monoClass)
				continue;

			if (MonoClass* parentClass = mono_class_get_parent(monoClass))
			{
				const char* parentName = mono_class_get_name(parentClass);
				const char* parentNamespace = mono_class_get_namespace(parentClass);

				const bool isEntity = monoClass && strcmp(parentName, "Entity") == 0 && strcmp(parentNamespace, "ArcEngine") == 0;
				if (isEntity)
					s_Data->EntityClasses[fullname] = CreateRef<ScriptClass>(nameSpace, name);
			}
		}
	}

	MonoImage* ScriptEngine::GetCoreAssemblyImage()
	{
		return s_Data->CoreImage;
	}

	MonoImage* ScriptEngine::GetAppAssemblyImage()
	{
		return s_Data->AppImage;
	}

	ScriptInstance* ScriptEngine::CreateInstance(Entity entity, const std::string& name)
	{
		ARC_PROFILE_SCOPE()

		const auto& scriptClass = s_Data->EntityClasses.at(name);
		const UUID entityID = entity.GetUUID();
		auto* instance = new ScriptInstance(scriptClass, entityID);
		s_Data->EntityRuntimeInstances[entityID][name] = instance;
		return instance;
	}

	bool ScriptEngine::HasInstance(Entity entity, const std::string& name)
	{
		ARC_PROFILE_SCOPE()

		return s_Data->EntityRuntimeInstances[entity.GetUUID()].contains(name);
	}

	ScriptInstance* ScriptEngine::GetInstance(Entity entity, const std::string& name)
	{
		ARC_PROFILE_SCOPE()

		return s_Data->EntityRuntimeInstances.at(entity.GetUUID()).at(name);
	}

	bool ScriptEngine::HasClass(const std::string& className)
	{
		ARC_PROFILE_SCOPE()

		return s_Data->EntityClasses.contains(className);
	}

	void ScriptEngine::RemoveInstance(Entity entity, const std::string& name)
	{
		ARC_PROFILE_SCOPE()

		delete s_Data->EntityRuntimeInstances.at(entity.GetUUID()).at(name);
		s_Data->EntityRuntimeInstances[entity.GetUUID()].erase(name);
	}

	MonoDomain* ScriptEngine::GetDomain()
	{
		return s_Data->AppDomain;
	}

	const std::vector<std::string>& ScriptEngine::GetFields(const char* className)
	{
		return s_Data->EntityClasses.at(className)->GetFields();
	}

	const std::unordered_map<std::string, ScriptField, UM_StringTransparentEquality>& ScriptEngine::GetFieldMap(const char* className)
	{
		return s_Data->EntityClasses.at(className)->GetFieldsMap();
	}

	std::unordered_map<std::string, ScriptFieldInstance, UM_StringTransparentEquality>& ScriptEngine::GetFieldInstanceMap(Entity entity, const char* className)
	{
		return s_Data->EntityFields[entity.GetUUID()][className];
	}

	std::unordered_map<std::string, Ref<ScriptClass>, UM_StringTransparentEquality>& ScriptEngine::GetClasses()
	{
		return s_Data->EntityClasses;
	}

	////////////////////////////////////////////////////////////////////////
	// Script Class ////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	ScriptClass::ScriptClass(MonoClass* monoClass, bool loadFields)
		: m_MonoClass(monoClass)
	{
		ARC_PROFILE_SCOPE()

		if (loadFields)
			LoadFields();
	}

	ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className)
		: m_ClassNamespace(classNamespace), m_ClassName(className)
	{
		ARC_PROFILE_SCOPE()

		m_MonoClass = mono_class_from_name(s_Data->AppImage, classNamespace.c_str(), className.c_str());
		LoadFields();
	}

	GCHandle ScriptClass::Instantiate() const
	{
		ARC_PROFILE_SCOPE()

		if (MonoObject* object = mono_object_new(s_Data->AppDomain, m_MonoClass))
		{
			mono_runtime_object_init(object);
			return GCManager::CreateObjectReference(object, false);
		}

		return 0;
	}

	MonoMethod* ScriptClass::GetMethod(const char* methodName, uint32_t parameterCount) const
	{
		ARC_PROFILE_SCOPE()

		return mono_class_get_method_from_name(m_MonoClass, methodName, static_cast<int>(parameterCount));
	}

	GCHandle ScriptClass::InvokeMethod(GCHandle gcHandle, MonoMethod* method, void** params) const
	{
		ARC_PROFILE_SCOPE()

		MonoObject* reference = GCManager::GetReferencedObject(gcHandle);
		MonoObject* exception;
		if (!reference)
		{
			ARC_APP_CRITICAL("System.NullReferenceException: Object reference not set to an instance of an object.");
			return gcHandle;
		}

		mono_runtime_invoke(method, reference, params, &exception);
		if (exception)
		{
			MonoString* monoString = mono_object_to_string(exception, nullptr);
			const std::string ex = MonoUtils::MonoStringToUTF8(monoString);
			ARC_APP_CRITICAL(ex);
		}
		return gcHandle;
	}

	enum class Accessibility : uint8_t
	{
		None = 0,
		Private = (1 << 0),
		Internal = (1 << 1),
		Protected = (1 << 2),
		Public = (1 << 3)
	};

	// Gets the accessibility level of the given field
	static uint8_t GetFieldAccessibility(MonoClassField* field)
	{
		ARC_PROFILE_SCOPE()

		auto accessibility = static_cast<uint8_t>(Accessibility::None);
		const uint32_t accessFlag = mono_field_get_flags(field) & MONO_FIELD_ATTR_FIELD_ACCESS_MASK;

		switch (accessFlag)
		{
			case MONO_FIELD_ATTR_PRIVATE:
			{
				accessibility = static_cast<uint8_t>(Accessibility::Private);
				break;
			}
			case MONO_FIELD_ATTR_FAM_AND_ASSEM:
			{
				accessibility |= static_cast<uint8_t>(Accessibility::Protected);
				accessibility |= static_cast<uint8_t>(Accessibility::Internal);
				break;
			}
			case MONO_FIELD_ATTR_ASSEMBLY:
			{
				accessibility = static_cast<uint8_t>(Accessibility::Internal);
				break;
			}
			case MONO_FIELD_ATTR_FAMILY:
			{
				accessibility = static_cast<uint8_t>(Accessibility::Protected);
				break;
			}
			case MONO_FIELD_ATTR_FAM_OR_ASSEM:
			{
				accessibility |= static_cast<uint8_t>(Accessibility::Private);
				accessibility |= static_cast<uint8_t>(Accessibility::Protected);
				break;
			}
			case MONO_FIELD_ATTR_PUBLIC:
			{
				accessibility = static_cast<uint8_t>(Accessibility::Public);
				break;
			}
		}

		return accessibility;
	}

	void ScriptClass::LoadFields()
	{
		ARC_PROFILE_SCOPE()

		m_Fields.clear();
		m_FieldsMap.clear();

		MonoObject* tempObject = mono_object_new(s_Data->AppDomain, m_MonoClass);
		const GCHandle tempObjectHandle = GCManager::CreateObjectReference(tempObject, false);
		mono_runtime_object_init(tempObject);

		MonoClassField* monoField;
		void* ptr = nullptr;
		while ((monoField = mono_class_get_fields(m_MonoClass, &ptr)) != nullptr)
		{
			const char* fieldName = mono_field_get_name(monoField);

			MonoType* fieldType = mono_field_get_type(monoField);

			FieldType type = FieldType::Unknown;
			{
				char* typeName = mono_type_get_name(fieldType);
				const auto& fieldIt = s_ScriptFieldTypeMap.find(typeName);
				if (fieldIt != s_ScriptFieldTypeMap.end())
					type = fieldIt->second;

				if (type == FieldType::Unknown)
				{
					ARC_CORE_WARN("Unsupported Field Type Name: {}", typeName);
					continue;
				}
				mono_free(typeName);
			}

			const uint8_t accessibilityFlag = GetFieldAccessibility(monoField);
			bool serializable = accessibilityFlag & static_cast<uint8_t>(Accessibility::Public);
			bool hidden = !serializable;
			std::string header;
			std::string tooltip;
			float min = 0;
			float max = 0;

			if (MonoCustomAttrInfo* attr = mono_custom_attrs_from_field(m_MonoClass, monoField))
			{
				if (!serializable)
					serializable = mono_custom_attrs_has_attr(attr, s_Data->SerializeFieldAttribute);

				hidden = !serializable;

				if (mono_custom_attrs_has_attr(attr, s_Data->HideInPropertiesAttribute))
					hidden = true;
				else if (mono_custom_attrs_has_attr(attr, s_Data->ShowInPropertiesAttribute))
					hidden = false;

				if (mono_custom_attrs_has_attr(attr, s_Data->HeaderAttribute))
				{
					MonoObject* attributeObject = mono_custom_attrs_get_attr(attr, s_Data->HeaderAttribute);
					MonoClassField* messageField = mono_class_get_field_from_name(s_Data->HeaderAttribute, "Message");
					MonoObject* monoStr = mono_field_get_value_object(ScriptEngine::GetDomain(), messageField, attributeObject);
					header = MonoUtils::MonoStringToUTF8(reinterpret_cast<MonoString*>(monoStr));
				}

				if (mono_custom_attrs_has_attr(attr, s_Data->TooltipAttribute))
				{
					MonoObject* attributeObject = mono_custom_attrs_get_attr(attr, s_Data->TooltipAttribute);
					MonoClassField* messageField = mono_class_get_field_from_name(s_Data->TooltipAttribute, "Message");
					MonoObject* monoStr = mono_field_get_value_object(ScriptEngine::GetDomain(), messageField, attributeObject);
					tooltip = MonoUtils::MonoStringToUTF8(reinterpret_cast<MonoString*>(monoStr));
				}

				if (mono_custom_attrs_has_attr(attr, s_Data->RangeAttribute))
				{
					MonoObject* attributeObject = mono_custom_attrs_get_attr(attr, s_Data->RangeAttribute);
					MonoClassField* minField = mono_class_get_field_from_name(s_Data->RangeAttribute, "Min");
					MonoClassField* maxField = mono_class_get_field_from_name(s_Data->RangeAttribute, "Max");
					mono_field_get_value(attributeObject, minField, &min);
					mono_field_get_value(attributeObject, maxField, &max);
				}
			}

			auto& scriptField = m_FieldsMap[fieldName];
			scriptField.Name = fieldName;
			if (scriptField.Name.size() > 1 && scriptField.Name[0] == '_')
				scriptField.DisplayName = &scriptField.Name[1];
			else if (scriptField.Name.size() > 2 && scriptField.Name[1] == '_')
				scriptField.DisplayName = &scriptField.Name[2];
			else
				scriptField.DisplayName = scriptField.Name;
			scriptField.Type = type;
			scriptField.Field = monoField;
			scriptField.Serializable = serializable;
			scriptField.Hidden = hidden;
			scriptField.Header = header;
			scriptField.Tooltip = tooltip;
			scriptField.Min = min;
			scriptField.Max = max;

			if (type == FieldType::String)
			{
				auto* monoStr = reinterpret_cast<MonoString*>(mono_field_get_value_object(s_Data->AppDomain, monoField, tempObject));
				std::string str = MonoUtils::MonoStringToUTF8(monoStr);
				memcpy(scriptField.DefaultValue, str.data(), sizeof(scriptField.DefaultValue));
			}
			else
			{
				mono_field_get_value(tempObject, monoField, scriptField.DefaultValue);
			}

			m_Fields.emplace_back(fieldName);
		}

		GCManager::ReleaseObjectReference(tempObjectHandle);
	}

	////////////////////////////////////////////////////////////////////////
	// Script Instance /////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////

	ScriptInstance::ScriptInstance(const Ref<ScriptClass>& scriptClass, UUID entityID)
		: m_ScriptClass(scriptClass)
	{
		ARC_PROFILE_SCOPE()

		m_Handle = scriptClass->Instantiate();

		m_EntityClass = CreateRef<ScriptClass>(s_Data->EntityClass);
		m_Constructor = m_EntityClass->GetMethod(".ctor", 1);
		void* params = &entityID;
		m_EntityClass->InvokeMethod(m_Handle, m_Constructor, &params);
		
		const std::string fullClassName = fmt::format("{}.{}", scriptClass->m_ClassNamespace, scriptClass->m_ClassName);
		auto& fieldsMap = s_Data->EntityFields[entityID][fullClassName];
		for (const auto& [fieldName, _] : scriptClass->m_FieldsMap)
		{
			const auto& fieldIt = fieldsMap.find(fieldName);
			if (fieldIt != fieldsMap.end())
			{
				const ScriptFieldInstance& fieldInstance = fieldIt->second;
				SetFieldValueInternal(fieldName, fieldInstance.GetBuffer());
			}
		}

		m_OnCreateMethod = scriptClass->GetMethod("OnCreate", 0);
		m_OnUpdateMethod = scriptClass->GetMethod("OnUpdate", 1);
		m_OnDestroyMethod = scriptClass->GetMethod("OnDestroy", 0);

		m_OnCollisionEnter2DMethod = m_EntityClass->GetMethod("HandleOnCollisionEnter2D", 1);
		m_OnCollisionExit2DMethod = m_EntityClass->GetMethod("HandleOnCollisionExit2D", 1);
		m_OnSensorEnter2DMethod = m_EntityClass->GetMethod("HandleOnSensorEnter2D", 1);
		m_OnSensorExit2DMethod = m_EntityClass->GetMethod("HandleOnSensorExit2D", 1);
	}

	ScriptInstance::~ScriptInstance()
	{
		ARC_PROFILE_SCOPE()

		GCManager::ReleaseObjectReference(m_Handle);
	}

	void ScriptInstance::InvokeOnCreate() const
	{
		ARC_PROFILE_SCOPE()

		if (m_OnCreateMethod)
			m_ScriptClass->InvokeMethod(m_Handle, m_OnCreateMethod);
	}

	void ScriptInstance::InvokeOnUpdate(float ts) const
	{
		ARC_PROFILE_SCOPE()

		if (m_OnUpdateMethod)
		{
			void* params = &ts;
			m_ScriptClass->InvokeMethod(m_Handle, m_OnUpdateMethod, &params);
		}
	}

	void ScriptInstance::InvokeOnDestroy() const
	{
		ARC_PROFILE_SCOPE()

		if (m_OnDestroyMethod)
			m_ScriptClass->InvokeMethod(m_Handle, m_OnDestroyMethod);
	}

	void ScriptInstance::InvokeOnCollisionEnter2D(Collision2DData& other) const
	{
		ARC_PROFILE_SCOPE()

		void* params = &other;
		m_EntityClass->InvokeMethod(m_Handle, m_OnCollisionEnter2DMethod, &params);
	}

	void ScriptInstance::InvokeOnCollisionExit2D(Collision2DData& other) const
	{
		ARC_PROFILE_SCOPE()

		void* params = &other;
		m_EntityClass->InvokeMethod(m_Handle, m_OnCollisionExit2DMethod, &params);
	}

	void ScriptInstance::InvokeOnSensorEnter2D(Collision2DData& other) const
	{
		ARC_PROFILE_SCOPE()

		void* params = &other;
		m_EntityClass->InvokeMethod(m_Handle, m_OnSensorEnter2DMethod, &params);
	}

	void ScriptInstance::InvokeOnSensorExit2D(Collision2DData& other) const
	{
		ARC_PROFILE_SCOPE()

		void* params = &other;
		m_EntityClass->InvokeMethod(m_Handle, m_OnSensorExit2DMethod, &params);
	}

    GCHandle ScriptInstance::GetHandle() const
    {
		return m_Handle;
    }

    void ScriptInstance::GetFieldValueInternal(const std::string& name, void* value) const
	{
		ARC_PROFILE_SCOPE()

		MonoClassField* classField = m_ScriptClass->m_FieldsMap.at(name).Field;
		mono_field_get_value(GCManager::GetReferencedObject(m_Handle), classField, value);
	}

	void ScriptInstance::SetFieldValueInternal(const std::string& name, const void* value) const
	{
		ARC_PROFILE_SCOPE()

		const auto& field = m_ScriptClass->m_FieldsMap.at(name);
		MonoClassField* classField = field.Field;
		if (field.Type == FieldType::String)
		{
			MonoString* monoStr = mono_string_new(s_Data->AppDomain, static_cast<const char*>(value));
			mono_field_set_value(GCManager::GetReferencedObject(m_Handle), classField, monoStr);
		}
		else
		{
			mono_field_set_value(GCManager::GetReferencedObject(m_Handle), classField, const_cast<void*>(value));
		}
	}

	std::string ScriptInstance::GetFieldValueStringInternal(const std::string& name) const
	{
		ARC_PROFILE_SCOPE()

		MonoClassField* classField = m_ScriptClass->m_FieldsMap.at(name).Field;
		auto* monoStr = reinterpret_cast<MonoString*>(mono_field_get_value_object(s_Data->AppDomain, classField, GCManager::GetReferencedObject(m_Handle)));
		return MonoUtils::MonoStringToUTF8(monoStr);
	}
}
