#pragma once

typedef struct _MonoType MonoType;
typedef struct _MonoClassField MonoClassField;

namespace ArcEngine
{
	using GCHandle = void*;
	using FieldData = void*;

	struct Field
	{
		enum class FieldType
		{
			Unknown = 0,
			Bool,
			Float,
			Double,
			SByte,
			Byte,
			Short,
			UShort,
			Int,
			UInt,
			Long,
			ULong,
			String,

			Vec2,
			Vec3,
			Vec4,
			Color
		};

		eastl::string Name;
		FieldType Type;
		bool Serializable;
		bool Hidden;
		eastl::string Header;
		eastl::string Tooltip;
		float Min = 0.0f;
		float Max = 0.0f;

		Field(const eastl::string& name, FieldType type, void* monoClassField, GCHandle handle);
		~Field();

		Field(const Field& other) = delete;
		Field(Field&& other) = delete;

		FieldData GetUnmanagedValue()
		{
			return (FieldData)m_Data.data();
		}

		template<typename T>
		T GetManagedValue() const
		{
			ARC_PROFILE_SCOPE();

			T value;
			GetManagedValueInternal(&value);
			return value;
		}

		void SetValue(FieldData value) const
		{
			memcpy((void*)m_Data.data(), value, m_Data.size());
			SetManagedValue(value);
		}

		eastl::string GetManagedValueString();
		void SetValueString(eastl::string& str);
		size_t GetSize() const { return m_Data.size(); }

		static FieldType GetFieldType(MonoType* monoType);

	private:

		void GetManagedValueInternal(FieldData outValue) const;

		void SetManagedValue(FieldData value) const;

		static FieldType GetFieldTypeFromValueType(MonoType* monoType);

		MonoClassField* m_Field;
		GCHandle m_Handle;
		eastl::vector<char> m_Data;
	};
}
