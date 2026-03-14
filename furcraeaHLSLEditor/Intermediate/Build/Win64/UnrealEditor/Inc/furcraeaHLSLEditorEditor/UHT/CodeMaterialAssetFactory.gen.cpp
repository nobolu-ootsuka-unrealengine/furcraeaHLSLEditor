// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "CodeMaterialAssetFactory.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeCodeMaterialAssetFactory() {}

// ********** Begin Cross Module References ********************************************************
FURCRAEAHLSLEDITOREDITOR_API UClass* Z_Construct_UClass_UCodeMaterialAssetFactory();
FURCRAEAHLSLEDITOREDITOR_API UClass* Z_Construct_UClass_UCodeMaterialAssetFactory_NoRegister();
UNREALED_API UClass* Z_Construct_UClass_UFactory();
UPackage* Z_Construct_UPackage__Script_furcraeaHLSLEditorEditor();
// ********** End Cross Module References **********************************************************

// ********** Begin Class UCodeMaterialAssetFactory ************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UCodeMaterialAssetFactory;
UClass* UCodeMaterialAssetFactory::GetPrivateStaticClass()
{
	using TClass = UCodeMaterialAssetFactory;
	if (!Z_Registration_Info_UClass_UCodeMaterialAssetFactory.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("CodeMaterialAssetFactory"),
			Z_Registration_Info_UClass_UCodeMaterialAssetFactory.InnerSingleton,
			StaticRegisterNativesUCodeMaterialAssetFactory,
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
	return Z_Registration_Info_UClass_UCodeMaterialAssetFactory.InnerSingleton;
}
UClass* Z_Construct_UClass_UCodeMaterialAssetFactory_NoRegister()
{
	return UCodeMaterialAssetFactory::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UCodeMaterialAssetFactory_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "CodeMaterialAssetFactory.h" },
		{ "ModuleRelativePath", "Public/CodeMaterialAssetFactory.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UCodeMaterialAssetFactory constinit property declarations ****************
// ********** End Class UCodeMaterialAssetFactory constinit property declarations ******************
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UCodeMaterialAssetFactory>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_UCodeMaterialAssetFactory_Statics
UObject* (*const Z_Construct_UClass_UCodeMaterialAssetFactory_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UFactory,
	(UObject* (*)())Z_Construct_UPackage__Script_furcraeaHLSLEditorEditor,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UCodeMaterialAssetFactory_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UCodeMaterialAssetFactory_Statics::ClassParams = {
	&UCodeMaterialAssetFactory::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UCodeMaterialAssetFactory_Statics::Class_MetaDataParams), Z_Construct_UClass_UCodeMaterialAssetFactory_Statics::Class_MetaDataParams)
};
void UCodeMaterialAssetFactory::StaticRegisterNativesUCodeMaterialAssetFactory()
{
}
UClass* Z_Construct_UClass_UCodeMaterialAssetFactory()
{
	if (!Z_Registration_Info_UClass_UCodeMaterialAssetFactory.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UCodeMaterialAssetFactory.OuterSingleton, Z_Construct_UClass_UCodeMaterialAssetFactory_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UCodeMaterialAssetFactory.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UCodeMaterialAssetFactory);
UCodeMaterialAssetFactory::~UCodeMaterialAssetFactory() {}
// ********** End Class UCodeMaterialAssetFactory **************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_Sandbox_UE573petit25Cl_UE573petit25Cl_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditorEditor_Public_CodeMaterialAssetFactory_h__Script_furcraeaHLSLEditorEditor_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UCodeMaterialAssetFactory, UCodeMaterialAssetFactory::StaticClass, TEXT("UCodeMaterialAssetFactory"), &Z_Registration_Info_UClass_UCodeMaterialAssetFactory, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UCodeMaterialAssetFactory), 1922906007U) },
	};
}; // Z_CompiledInDeferFile_FID_Sandbox_UE573petit25Cl_UE573petit25Cl_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditorEditor_Public_CodeMaterialAssetFactory_h__Script_furcraeaHLSLEditorEditor_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Sandbox_UE573petit25Cl_UE573petit25Cl_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditorEditor_Public_CodeMaterialAssetFactory_h__Script_furcraeaHLSLEditorEditor_1598609414{
	TEXT("/Script/furcraeaHLSLEditorEditor"),
	Z_CompiledInDeferFile_FID_Sandbox_UE573petit25Cl_UE573petit25Cl_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditorEditor_Public_CodeMaterialAssetFactory_h__Script_furcraeaHLSLEditorEditor_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Sandbox_UE573petit25Cl_UE573petit25Cl_Plugins_furcraeaHLSLEditor_Source_furcraeaHLSLEditorEditor_Public_CodeMaterialAssetFactory_h__Script_furcraeaHLSLEditorEditor_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
