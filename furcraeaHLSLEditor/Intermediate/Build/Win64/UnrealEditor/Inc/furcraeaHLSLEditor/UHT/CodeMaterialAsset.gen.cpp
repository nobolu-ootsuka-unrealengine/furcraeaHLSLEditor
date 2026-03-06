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
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_HlslCode_MetaData[] = {
		{ "Category", "HLSL" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// \xef\xbf\xbdG\xef\xbf\xbd""f\xef\xbf\xbd""B\xef\xbf\xbd^\xef\xbf\xbd\xc5\x95\xd2\x8fW\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbdHLSL\xef\xbf\xbdR\xef\xbf\xbd[\xef\xbf\xbdh\xef\xbf\xbd{\xef\xbf\xbd\xef\xbf\xbd\n" },
#endif
		{ "ModuleRelativePath", "public/CodeMaterialAsset.h" },
		{ "MultiLine", "TRUE" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xef\xbf\xbdG\xef\xbf\xbd""f\xef\xbf\xbd""B\xef\xbf\xbd^\xef\xbf\xbd\xc5\x95\xd2\x8fW\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbdHLSL\xef\xbf\xbdR\xef\xbf\xbd[\xef\xbf\xbdh\xef\xbf\xbd{\xef\xbf\xbd\xef\xbf\xbd" },
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
	static const UECodeGen_Private::FStrPropertyParams NewProp_HlslCode;
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
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UCodeMaterialAsset_Statics::NewProp_HlslCode = { "HlslCode", nullptr, (EPropertyFlags)0x0010000000000001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UCodeMaterialAsset, HlslCode), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_HlslCode_MetaData), NewProp_HlslCode_MetaData) };
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_UCodeMaterialAsset_Statics::NewProp_OutputMaterial = { "OutputMaterial", nullptr, (EPropertyFlags)0x0114000000020001, UECodeGen_Private::EPropertyGenFlags::Object | UECodeGen_Private::EPropertyGenFlags::ObjectPtr, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UCodeMaterialAsset, OutputMaterial), Z_Construct_UClass_UMaterial_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OutputMaterial_MetaData), NewProp_OutputMaterial_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UCodeMaterialAsset_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UCodeMaterialAsset_Statics::NewProp_HlslCode,
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
struct Z_CompiledInDeferFile_FID_Sandbox_UE57CreatePlugin4_UE57HLSLEdit_UE57HLSLEdit_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditor_public_CodeMaterialAsset_h__Script_furcraeaHLSLEditor_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UCodeMaterialAsset, UCodeMaterialAsset::StaticClass, TEXT("UCodeMaterialAsset"), &Z_Registration_Info_UClass_UCodeMaterialAsset, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UCodeMaterialAsset), 676908126U) },
	};
}; // Z_CompiledInDeferFile_FID_Sandbox_UE57CreatePlugin4_UE57HLSLEdit_UE57HLSLEdit_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditor_public_CodeMaterialAsset_h__Script_furcraeaHLSLEditor_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Sandbox_UE57CreatePlugin4_UE57HLSLEdit_UE57HLSLEdit_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditor_public_CodeMaterialAsset_h__Script_furcraeaHLSLEditor_782999902{
	TEXT("/Script/furcraeaHLSLEditor"),
	Z_CompiledInDeferFile_FID_Sandbox_UE57CreatePlugin4_UE57HLSLEdit_UE57HLSLEdit_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditor_public_CodeMaterialAsset_h__Script_furcraeaHLSLEditor_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Sandbox_UE57CreatePlugin4_UE57HLSLEdit_UE57HLSLEdit_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditor_public_CodeMaterialAsset_h__Script_furcraeaHLSLEditor_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
