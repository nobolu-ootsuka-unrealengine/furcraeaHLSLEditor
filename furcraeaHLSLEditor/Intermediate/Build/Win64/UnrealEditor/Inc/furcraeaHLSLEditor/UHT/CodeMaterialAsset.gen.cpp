// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "furcraeaHLSLEditor/public/CodeMaterialAsset.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeCodeMaterialAsset() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UDataAsset();
ENGINE_API UClass* Z_Construct_UClass_UMaterial_NoRegister();
FURCRAEAHLSLEDITOR_API UClass* Z_Construct_UClass_UCodeMaterialAsset();
FURCRAEAHLSLEDITOR_API UClass* Z_Construct_UClass_UCodeMaterialAsset_NoRegister();
UPackage* Z_Construct_UPackage__Script_furcraeaHLSLEditor();
// ********** End Cross Module References **********************************************************

// ********** Begin Class UCodeMaterialAsset *******************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UCodeMaterialAsset;
UClass* UCodeMaterialAsset::GetPrivateStaticClass()
{
	using TClass = UCodeMaterialAsset;
	if (!Z_Registration_Info_UClass_UCodeMaterialAsset.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("CodeMaterialAsset"),
			Z_Registration_Info_UClass_UCodeMaterialAsset.InnerSingleton,
			StaticRegisterNativesUCodeMaterialAsset,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_UCodeMaterialAsset.InnerSingleton;
}
UClass* Z_Construct_UClass_UCodeMaterialAsset_NoRegister()
{
	return UCodeMaterialAsset::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UCodeMaterialAsset_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "IncludePath", "CodeMaterialAsset.h" },
		{ "ModuleRelativePath", "public/CodeMaterialAsset.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_FragmentShaderCode_MetaData[] = {
		{ "Category", "HLSL" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Fragment \xe3\x82\xb7\xe3\x82\xa7\xe3\x83\xbc\xe3\x83\x80\xe3\x83\xbc\xe3\x82\xb3\xe3\x83\xbc\xe3\x83\x89\xef\xbc\x88""FragmentShader \xe3\x82\xa8\xe3\x83\xb3\xe3\x83\x88\xe3\x83\xaa\xe9\x96\xa2\xe6\x95\xb0\xe3\x82\x92\xe5\x90\xab\xe3\x82\x80\xe3\x83\xbb\xe5\xbf\x85\xe9\xa0\x88\xef\xbc\x89\n" },
#endif
		{ "ModuleRelativePath", "public/CodeMaterialAsset.h" },
		{ "MultiLine", "TRUE" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Fragment \xe3\x82\xb7\xe3\x82\xa7\xe3\x83\xbc\xe3\x83\x80\xe3\x83\xbc\xe3\x82\xb3\xe3\x83\xbc\xe3\x83\x89\xef\xbc\x88""FragmentShader \xe3\x82\xa8\xe3\x83\xb3\xe3\x83\x88\xe3\x83\xaa\xe9\x96\xa2\xe6\x95\xb0\xe3\x82\x92\xe5\x90\xab\xe3\x82\x80\xe3\x83\xbb\xe5\xbf\x85\xe9\xa0\x88\xef\xbc\x89" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_VertexShaderCode_MetaData[] = {
		{ "Category", "HLSL" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Vertex \xe3\x82\xb7\xe3\x82\xa7\xe3\x83\xbc\xe3\x83\x80\xe3\x83\xbc\xe3\x82\xb3\xe3\x83\xbc\xe3\x83\x89\xef\xbc\x88VertexShader \xe3\x82\xa8\xe3\x83\xb3\xe3\x83\x88\xe3\x83\xaa\xe9\x96\xa2\xe6\x95\xb0\xe3\x82\x92\xe5\x90\xab\xe3\x82\x80\xe3\x83\xbb\xe7\x9c\x81\xe7\x95\xa5\xe5\x8f\xaf\xef\xbc\x89\n" },
#endif
		{ "ModuleRelativePath", "public/CodeMaterialAsset.h" },
		{ "MultiLine", "TRUE" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Vertex \xe3\x82\xb7\xe3\x82\xa7\xe3\x83\xbc\xe3\x83\x80\xe3\x83\xbc\xe3\x82\xb3\xe3\x83\xbc\xe3\x83\x89\xef\xbc\x88VertexShader \xe3\x82\xa8\xe3\x83\xb3\xe3\x83\x88\xe3\x83\xaa\xe9\x96\xa2\xe6\x95\xb0\xe3\x82\x92\xe5\x90\xab\xe3\x82\x80\xe3\x83\xbb\xe7\x9c\x81\xe7\x95\xa5\xe5\x8f\xaf\xef\xbc\x89" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_OutputMaterial_MetaData[] = {
		{ "Category", "HLSL" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// \xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xea\x82\xbdMaterial\xef\xbf\xbdi\xef\xbf\xbdL\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd""b\xef\xbf\xbdV\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbdj\n" },
#endif
		{ "ModuleRelativePath", "public/CodeMaterialAsset.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xea\x82\xbdMaterial\xef\xbf\xbdi\xef\xbf\xbdL\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd""b\xef\xbf\xbdV\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbdj" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UCodeMaterialAsset constinit property declarations ***********************
	static const UECodeGen_Private::FStrPropertyParams NewProp_FragmentShaderCode;
	static const UECodeGen_Private::FStrPropertyParams NewProp_VertexShaderCode;
	static const UECodeGen_Private::FObjectPropertyParams NewProp_OutputMaterial;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UCodeMaterialAsset constinit property declarations *************************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UCodeMaterialAsset>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_UCodeMaterialAsset_Statics

// ********** Begin Class UCodeMaterialAsset Property Definitions **********************************
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UCodeMaterialAsset_Statics::NewProp_FragmentShaderCode = { "FragmentShaderCode", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UCodeMaterialAsset, FragmentShaderCode), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_FragmentShaderCode_MetaData), NewProp_FragmentShaderCode_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UCodeMaterialAsset_Statics::NewProp_VertexShaderCode = { "VertexShaderCode", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UCodeMaterialAsset, VertexShaderCode), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_VertexShaderCode_MetaData), NewProp_VertexShaderCode_MetaData) };
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_UCodeMaterialAsset_Statics::NewProp_OutputMaterial = { "OutputMaterial", nullptr, (EPropertyFlags)0x0114000000020001, UECodeGen_Private::EPropertyGenFlags::Object | UECodeGen_Private::EPropertyGenFlags::ObjectPtr, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UCodeMaterialAsset, OutputMaterial), Z_Construct_UClass_UMaterial_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OutputMaterial_MetaData), NewProp_OutputMaterial_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UCodeMaterialAsset_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UCodeMaterialAsset_Statics::NewProp_FragmentShaderCode,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UCodeMaterialAsset_Statics::NewProp_VertexShaderCode,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UCodeMaterialAsset_Statics::NewProp_OutputMaterial,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UCodeMaterialAsset_Statics::PropPointers) < 2048);
// ********** End Class UCodeMaterialAsset Property Definitions ************************************
UObject* (*const Z_Construct_UClass_UCodeMaterialAsset_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UDataAsset,
	(UObject* (*)())Z_Construct_UPackage__Script_furcraeaHLSLEditor,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UCodeMaterialAsset_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UCodeMaterialAsset_Statics::ClassParams = {
	&UCodeMaterialAsset::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UCodeMaterialAsset_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UCodeMaterialAsset_Statics::PropPointers),
	0,
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UCodeMaterialAsset_Statics::Class_MetaDataParams), Z_Construct_UClass_UCodeMaterialAsset_Statics::Class_MetaDataParams)
};
void UCodeMaterialAsset::StaticRegisterNativesUCodeMaterialAsset()
{
}
UClass* Z_Construct_UClass_UCodeMaterialAsset()
{
	if (!Z_Registration_Info_UClass_UCodeMaterialAsset.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UCodeMaterialAsset.OuterSingleton, Z_Construct_UClass_UCodeMaterialAsset_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UCodeMaterialAsset.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UCodeMaterialAsset);
UCodeMaterialAsset::~UCodeMaterialAsset() {}
// ********** End Class UCodeMaterialAsset *********************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Sandbox_UE573petit25Cl_UE573petit25Cl_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditor_public_CodeMaterialAsset_h__Script_furcraeaHLSLEditor_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UCodeMaterialAsset, UCodeMaterialAsset::StaticClass, TEXT("UCodeMaterialAsset"), &Z_Registration_Info_UClass_UCodeMaterialAsset, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UCodeMaterialAsset), 1168972455U) },
	};
}; // Z_CompiledInDeferFile_FID_Sandbox_UE573petit25Cl_UE573petit25Cl_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditor_public_CodeMaterialAsset_h__Script_furcraeaHLSLEditor_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Sandbox_UE573petit25Cl_UE573petit25Cl_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditor_public_CodeMaterialAsset_h__Script_furcraeaHLSLEditor_3639567282{
	TEXT("/Script/furcraeaHLSLEditor"),
	Z_CompiledInDeferFile_FID_Sandbox_UE573petit25Cl_UE573petit25Cl_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditor_public_CodeMaterialAsset_h__Script_furcraeaHLSLEditor_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Sandbox_UE573petit25Cl_UE573petit25Cl_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditor_public_CodeMaterialAsset_h__Script_furcraeaHLSLEditor_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
