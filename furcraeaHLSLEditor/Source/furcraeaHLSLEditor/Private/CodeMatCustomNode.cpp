// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatCustomNode.cpp

#include "CodeMatCustomNode.h"

#if WITH_EDITOR

#include "CodeMatHLSLSource.h"

namespace CodeMatCustom
{

namespace
{
const TCHAR* ReturnTypeName(ECustomMaterialOutputType OutputType)
{
    switch (OutputType)
    {
    case CMOT_Float1: return TEXT("float");
    case CMOT_Float2: return TEXT("float2");
    case CMOT_Float4: return TEXT("float4");
    default:          return TEXT("float3");
    }
}

/** ヘルパー群 + 実装関数を、ラッパーより前に出力される #define の値として組み立てる */
FCustomDefine MakeGlobalCodeDefine(const FString& ImplName, const FString& ImplDefinition, const FString& Helpers)
{
    // "#define NAME 1" の改行以降はそのままグローバルスコープのコードになる
    FString Value = TEXT("1\n");
    Value += TEXT("// ---- helpers (furcraeaHLSLEditor) ----\n");
    Value += Helpers.TrimStartAndEnd() + TEXT("\n\n");
    Value += ImplDefinition;

    // 内容ハッシュ付きのガード名: 同一コードは 1 回だけ定義、異なるコードは衝突しない
    FCustomDefine Define;
    Define.DefineName  = FString::Printf(TEXT("CODEMAT_%s_%08X"), *ImplName.ToUpper(), FCrc::StrCrc32(*Value));
    Define.DefineValue = Value;
    return Define;
}
} // namespace

FCustomNodeSpec MakeSpec(const FString& Source, const FString& FuncName, ECustomMaterialOutputType OutputType, EStage Stage)
{
    FCustomNodeSpec Spec;
    Spec.OutputType  = OutputType;
    Spec.Description = FuncName;
    Spec.Inputs      = CodeMatSource::ExtractFuncParams(Source, FuncName);

    FString Body = CodeMatSource::ExtractFunctionBody(Source, FuncName);
    if (Body.IsEmpty())
    {
        Spec.Error = FString::Printf(TEXT("could not extract the body of '%s'"), *FuncName);
        Spec.Code  = FString::Printf(TEXT("// ERROR: %s\nreturn 0;\n"), *Spec.Error);
        return Spec;
    }

    // Time を引数で受け取っていなければ、裸の Time を View.GameTime に束縛する
    const bool bHasTimeInput = Spec.Inputs.ContainsByPredicate(
        [](const TPair<FString, FString>& In) { return In.Value.Equals(TEXT("Time"), ESearchCase::IgnoreCase); });
    if (!bHasTimeInput) Body = CodeMatSource::InjectHLSLBuiltins(Body);

    const FString Helpers = CodeMatSource::ExtractHelperFunctions(Source, FuncName);
    if (Helpers.IsEmpty())
    {
        // ヘルパーなし: ボディをそのまま埋め込む (Parameters も直接使える)
        Spec.Code = Body;
        return Spec;
    }

    // ヘルパーあり: 実装関数をグローバルへ置き、ラッパーからは呼ぶだけにする。
    // 実装関数にも Parameters を渡すので、ボディ内の GetDefaultSceneTextureUV(Parameters, ...) 等も動く。
    const FString ImplName = FuncName + TEXT("_impl");
    FString ImplParams = (Stage == EStage::Vertex) ? TEXT("FMaterialVertexParameters Parameters") : TEXT("FMaterialPixelParameters Parameters");
    FString CallArgs   = TEXT("Parameters");
    for (const TPair<FString, FString>& In : Spec.Inputs)
    {
        ImplParams += FString::Printf(TEXT(", %s %s"), *In.Key, *In.Value);
        CallArgs   += TEXT(", ") + In.Value;
    }

    const FString ImplDefinition = FString::Printf(TEXT("%s %s(%s)\n{\n%s\n}\n"),
        ReturnTypeName(OutputType), *ImplName, *ImplParams, *Body.TrimStartAndEnd());

    Spec.Defines.Add(MakeGlobalCodeDefine(ImplName, ImplDefinition, Helpers));
    Spec.Code = FString::Printf(TEXT("return %s(%s);"), *ImplName, *CallArgs);
    return Spec;
}

void ApplySpec(UMaterialExpressionCustom* Custom, const FCustomNodeSpec& Spec)
{
    Custom->OutputType  = Spec.OutputType;
    Custom->Description = Spec.Description;
    Custom->Code        = Spec.Code;
    // ShaderCompileWorker がプラグインの仮想パスを解決できないため #include は使わない
    Custom->IncludeFilePaths.Reset();
    Custom->AdditionalDefines = Spec.Defines;
    Custom->Inputs.Reset();
    for (const TPair<FString, FString>& In : Spec.Inputs)
        Custom->Inputs.AddDefaulted_GetRef().InputName = FName(*In.Value);
}

} // namespace CodeMatCustom

#endif // WITH_EDITOR
