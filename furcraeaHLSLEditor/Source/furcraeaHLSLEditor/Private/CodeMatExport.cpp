// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatExport.cpp
//
// 生成グラフを他の形式で受け渡す:
//   - Python : 同じグラフを MaterialEditingLibrary で再構築するスクリプト
//   - T3D    : マテリアルエディタのコピー形式 (Ctrl+V で貼れる)
//   - Paste  : T3D をマテリアルへ貼り付け、"-> Emissive" 等のコメントから出力ピンへ接続

#include "CodeMaterialCompiler.h"

#if WITH_EDITOR

#include "CodeMatGraphBuilder.h"
#include "CodeMatGraphUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionCustom.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "EdGraphUtilities.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
using CodeMatGraph::FOutputPin;

/** コンパイルしない一時マテリアルにグラフを構築する (呼び出し側で MarkAsGarbage すること) */
UMaterial* BuildTransientMaterial(const FCodeMatBuildSettings& S, FString& OutLog)
{
    UPackage* Pkg = GetTransientPackage();
    const FName Name = MakeUniqueObjectName(Pkg, UMaterial::StaticClass(), FName(*FString::Printf(TEXT("M_%s_Export"), *S.AssetName)));
    UMaterial* Mat = NewObject<UMaterial>(Pkg, Name, RF_Transient);
    if (!Mat) return nullptr;

    CodeMatGraph::BuildMaterialGraph(Mat, S, /*bFinalize*/ false, OutLog);
    CodeMatGraph::ReplaceInputMasksWithNodes(Mat);
    return Mat;
}

FString OutputName(UMaterialExpression* E, int32 OutputIndex)
{
    TArray<FExpressionOutput>& Outs = E->GetOutputs();
    if (Outs.IsValidIndex(OutputIndex) && !Outs[OutputIndex].OutputName.IsNone())
        return Outs[OutputIndex].OutputName.ToString();
    return FString();
}

/** マテリアルエディタ / MaterialEditingLibrary と同じ短縮ピン名 ("Coordinates" → "UVs" 等) */
FString InputName(UMaterialExpression* E, int32 InputIndex)
{
    const FName N = UMaterialGraphNode::GetShortenPinName(E->GetInputName(InputIndex));
    return N.IsNone() ? FString() : N.ToString();
}

// ============================================================================
// Python 文字列化 (UE の PythonizeName と同じ規則)
// ============================================================================

FString PyCase(const FString& In, bool bUpper)
{
    FString Out;
    for (int32 i = 0; i < In.Len(); ++i)
    {
        const TCHAR c = In[i];
        if (i > 0 && FChar::IsUpper(c))
        {
            const TCHAR Prev = In[i - 1];
            const TCHAR Next = (i + 1 < In.Len()) ? In[i + 1] : 0;
            if (Prev != TEXT('_') && (FChar::IsLower(Prev) || FChar::IsDigit(Prev) || (FChar::IsUpper(Prev) && FChar::IsLower(Next))))
                Out += TEXT('_');
        }
        Out += c;
    }
    return bUpper ? Out.ToUpper() : Out.ToLower();
}

FString PyPropertyName(const FProperty* Prop)
{
    FString N = Prop->GetName();
    if (Prop->IsA<FBoolProperty>() && N.Len() > 1 && N[0] == TEXT('b') && FChar::IsUpper(N[1]))
        N.RightChopInline(1);
    return PyCase(N, false);
}

FString PyQuote(const FString& S)
{
    FString O = TEXT("\"");
    for (TCHAR c : S)
    {
        switch (c)
        {
        case TEXT('\\'): O += TEXT("\\\\"); break;
        case TEXT('"'):  O += TEXT("\\\""); break;
        case TEXT('\n'): O += TEXT("\\n");  break;
        case TEXT('\r'): break;
        case TEXT('\t'): O += TEXT("\\t");  break;
        default:
            if (c < 32 || c > 126) O += FString::Printf(TEXT("\\u%04x"), (uint32)c);
            else                   O += c;
            break;
        }
    }
    return O + TEXT("\"");
}

FString PyFloat(double V)
{
    FString S = FString::Printf(TEXT("%.9g"), V);
    if (!S.Contains(TEXT(".")) && !S.Contains(TEXT("e")) && !S.Contains(TEXT("n")) && !S.Contains(TEXT("i"))) S += TEXT(".0");
    return S;
}

bool PyEnum(const UEnum* Enum, int64 Value, FString& Out)
{
    if (!Enum) return false;
    const FString ValueName = Enum->GetNameStringByValue(Value);
    if (ValueName.IsEmpty()) return false;
    FString EnumName = Enum->GetName();
    if (EnumName.Len() > 1 && EnumName[0] == TEXT('E') && FChar::IsUpper(EnumName[1])) EnumName.RightChopInline(1);
    Out = FString::Printf(TEXT("unreal.%s.%s"), *EnumName, *PyCase(ValueName, true));
    return true;
}

bool IsEnumProperty(const FProperty* Prop)
{
    if (Prop->IsA<FEnumProperty>()) return true;
    const FByteProperty* BP = CastField<FByteProperty>(Prop);
    return BP && BP->Enum;
}

bool PyValue(const FProperty* Prop, const void* Ptr, FString& Out)
{
    if (const FBoolProperty* BP = CastField<FBoolProperty>(Prop))
    {
        Out = BP->GetPropertyValue(Ptr) ? TEXT("True") : TEXT("False");
        return true;
    }
    if (const FEnumProperty* EP = CastField<FEnumProperty>(Prop))
        return PyEnum(EP->GetEnum(), EP->GetUnderlyingProperty()->GetSignedIntPropertyValue(Ptr), Out);
    if (const FByteProperty* ByP = CastField<FByteProperty>(Prop); ByP && ByP->Enum)
        return PyEnum(ByP->Enum, ByP->GetPropertyValue(Ptr), Out);
    if (const FNumericProperty* NP = CastField<FNumericProperty>(Prop))
    {
        Out = NP->IsFloatingPoint() ? PyFloat(NP->GetFloatingPointPropertyValue(Ptr))
                                    : FString::Printf(TEXT("%lld"), NP->GetSignedIntPropertyValue(Ptr));
        return true;
    }
    if (const FNameProperty* NmP = CastField<FNameProperty>(Prop)) { Out = PyQuote(NmP->GetPropertyValue(Ptr).ToString()); return true; }
    if (const FStrProperty* SP = CastField<FStrProperty>(Prop))    { Out = PyQuote(SP->GetPropertyValue(Ptr)); return true; }
    if (const FStructProperty* StP = CastField<FStructProperty>(Prop))
    {
        if (StP->Struct == TBaseStructure<FLinearColor>::Get())
        {
            const FLinearColor& C = *static_cast<const FLinearColor*>(Ptr);
            Out = FString::Printf(TEXT("unreal.LinearColor(%s, %s, %s, %s)"), *PyFloat(C.R), *PyFloat(C.G), *PyFloat(C.B), *PyFloat(C.A));
            return true;
        }
        if (StP->Struct == TBaseStructure<FVector2D>::Get())
        {
            const FVector2D& V = *static_cast<const FVector2D*>(Ptr);
            Out = FString::Printf(TEXT("unreal.Vector2D(%s, %s)"), *PyFloat(V.X), *PyFloat(V.Y));
            return true;
        }
        if (StP->Struct == TBaseStructure<FVector>::Get())
        {
            const FVector& V = *static_cast<const FVector*>(Ptr);
            Out = FString::Printf(TEXT("unreal.Vector(%s, %s, %s)"), *PyFloat(V.X), *PyFloat(V.Y), *PyFloat(V.Z));
            return true;
        }
        return false;
    }
    if (const FObjectPropertyBase* OP = CastField<FObjectPropertyBase>(Prop))
    {
        const UObject* Obj = OP->GetObjectPropertyValue(Ptr);
        Out = Obj ? FString::Printf(TEXT("unreal.load_asset(%s)"), *PyQuote(Obj->GetPathName())) : TEXT("None");
        return true;
    }
    return false;
}

/** Python で書き出すべきプロパティか */
bool ShouldExportProperty(const UMaterialExpression* E, const FProperty* Prop, const UObject* CDO)
{
    if (!Prop->HasAnyPropertyFlags(CPF_Edit) || Prop->HasAnyPropertyFlags(CPF_EditConst | CPF_Deprecated | CPF_Transient)) return false;
    if (Prop->GetOwnerClass() == UMaterialExpression::StaticClass() && Prop->GetFName() != TEXT("Desc")) return false;
    if (Prop->ArrayDim != 1) return false;
    if (const FStructProperty* SP = CastField<FStructProperty>(Prop); SP && SP->Struct->GetFName() == TEXT("ExpressionInput")) return false;
    if (E->IsA<UMaterialExpressionCustom>() &&
        (Prop->GetFName() == TEXT("Inputs") || Prop->GetFName() == TEXT("AdditionalDefines"))) return false;  // 別途出力

    // MEL.create_material_expression はクラス既定値と異なる初期値を入れることがある
    // (例: ComponentMask は R,G が最初から True)。bool / enum は常に明示する。
    const bool bAlwaysWrite = Prop->IsA<FBoolProperty>() || IsEnumProperty(Prop);
    return bAlwaysWrite || !Prop->Identical_InContainer(E, CDO);
}

const TCHAR* PythonPrelude = TEXT(
    "def _custom_input(name):\n"
    "    ci = unreal.CustomInput()\n"
    "    ci.set_editor_property(\"input_name\", name)\n"
    "    return ci\n"
    "\n"
    "def _custom_define(name, value):\n"
    "    cd = unreal.CustomDefine()\n"
    "    cd.set_editor_property(\"define_name\", name)\n"
    "    cd.set_editor_property(\"define_value\", value)\n"
    "    return cd\n"
    "\n"
    "def _get_or_create_material(asset_dir, asset_name):\n"
    "    path = asset_dir + \"/\" + asset_name\n"
    "    if unreal.EditorAssetLibrary.does_asset_exist(path):\n"
    "        m = unreal.EditorAssetLibrary.load_asset(path)\n"
    "        MEL.delete_all_material_expressions(m)\n"
    "        return m\n"
    "    tools = unreal.AssetToolsHelpers.get_asset_tools()\n"
    "    return tools.create_asset(asset_name, asset_dir, unreal.Material, unreal.MaterialFactoryNew())\n"
    "\n"
    "mat = _get_or_create_material(ASSET_DIR, ASSET_NAME)\n");

void AppendMaterialSettings(FString& P, UMaterial* Mat)
{
    static const TCHAR* MatProps[] = {
        TEXT("MaterialDomain"), TEXT("BlendMode"), TEXT("ShadingModel"), TEXT("TwoSided"), TEXT("BlendableLocation"),
        TEXT("bUsedWithSkeletalMesh"), TEXT("bUsedWithStaticMesh"), TEXT("bAutomaticallySetUsageInEditor"), TEXT("bCastRayTracedShadows"),
    };
    P += TEXT("\n# ---- material settings ----\n");
    for (const TCHAR* PropName : MatProps)
    {
        const FProperty* Prop = FindFProperty<FProperty>(UMaterial::StaticClass(), PropName);
        FString Val;
        if (Prop && PyValue(Prop, Prop->ContainerPtrToValuePtr<void>(Mat), Val))
            P += FString::Printf(TEXT("mat.set_editor_property(%s, %s)\n"), *PyQuote(PyPropertyName(Prop)), *Val);
    }
}

void AppendNodes(FString& P, const TArray<UMaterialExpression*>& Exprs)
{
    P += FString::Printf(TEXT("\n# ---- nodes (%d) ----\nn = {}\n"), Exprs.Num());
    for (int32 i = 0; i < Exprs.Num(); ++i)
    {
        UMaterialExpression* E = Exprs[i];
        UClass* Cls = E->GetClass();
        P += FString::Printf(TEXT("n[%d] = MEL.create_material_expression(mat, unreal.%s, %d, %d)\n"),
            i, *Cls->GetName(), E->MaterialExpressionEditorX, E->MaterialExpressionEditorY);

        const UObject* CDO = Cls->GetDefaultObject();
        for (TFieldIterator<FProperty> It(Cls); It; ++It)
        {
            const FProperty* Prop = *It;
            if (!ShouldExportProperty(E, Prop, CDO)) continue;
            FString Val;
            if (PyValue(Prop, Prop->ContainerPtrToValuePtr<void>(E), Val))
                P += FString::Printf(TEXT("n[%d].set_editor_property(%s, %s)\n"), i, *PyQuote(PyPropertyName(Prop)), *Val);
            else
                P += FString::Printf(TEXT("# n[%d]: property '%s' is not exported\n"), i, *Prop->GetName());
        }

        if (const UMaterialExpressionCustom* C = Cast<UMaterialExpressionCustom>(E))
        {
            FString List;
            for (const FCustomInput& CI : C->Inputs)
                List += (List.IsEmpty() ? TEXT("") : TEXT(", ")) + FString::Printf(TEXT("_custom_input(%s)"), *PyQuote(CI.InputName.ToString()));
            P += FString::Printf(TEXT("n[%d].set_editor_property(\"inputs\", [%s])\n"), i, *List);

            if (C->AdditionalDefines.Num() > 0)
            {
                FString Defines;
                for (const FCustomDefine& D : C->AdditionalDefines)
                    Defines += (Defines.IsEmpty() ? TEXT("") : TEXT(", ")) +
                               FString::Printf(TEXT("_custom_define(%s, %s)"), *PyQuote(D.DefineName), *PyQuote(D.DefineValue));
                P += FString::Printf(TEXT("n[%d].set_editor_property(\"additional_defines\", [%s])\n"), i, *Defines);
            }
        }
    }
}

void AppendConnections(FString& P, UMaterialEditorOnlyData* ED, const TArray<UMaterialExpression*>& Exprs)
{
    TMap<UMaterialExpression*, int32> Index;
    for (int32 i = 0; i < Exprs.Num(); ++i) Index.Add(Exprs[i], i);

    P += TEXT("\n# ---- connections ----\n");
    for (int32 i = 0; i < Exprs.Num(); ++i)
    {
        UMaterialExpression* E = Exprs[i];
        for (FExpressionInputIterator It{ E }; It; ++It)
        {
            const int32* Src = It.Input->Expression ? Index.Find(It.Input->Expression) : nullptr;
            if (!Src) continue;
            P += FString::Printf(TEXT("MEL.connect_material_expressions(n[%d], %s, n[%d], %s)\n"),
                *Src, *PyQuote(OutputName(It.Input->Expression, It.Input->OutputIndex)), i, *PyQuote(InputName(E, It.Index)));
        }
    }
    for (const FOutputPin& Pin : CodeMatGraph::GetMaterialOutputPins(ED))
    {
        const int32* Src = Pin.Key->Expression ? Index.Find(Pin.Key->Expression) : nullptr;
        FString PropEnum;
        if (!Src || !PyEnum(StaticEnum<EMaterialProperty>(), (int64)Pin.Value, PropEnum)) continue;
        P += FString::Printf(TEXT("MEL.connect_material_property(n[%d], %s, %s)\n"),
            *Src, *PyQuote(OutputName(Pin.Key->Expression, Pin.Key->OutputIndex)), *PropEnum);
    }
}

/** "-> Emissive (RGB)" 形式のラベル。T3D ではルートノードへの接続がコピーされないため。 */
FString MakeOutputLabel(UMaterialExpression* E, const FOutputPin& Pin)
{
    FString Label = CodeMatGraph::GetOutputPinDisplayName(Pin.Value);
    if (Pin.Key->OutputIndex > 0)
    {
        const FString OutName = OutputName(E, Pin.Key->OutputIndex);
        Label += FString::Printf(TEXT(" (%s)"), OutName.IsEmpty() ? *FString::FromInt(Pin.Key->OutputIndex) : *OutName);
    }
    return TEXT("-> ") + Label;
}

/** MakeOutputLabel の逆: Desc の各ラベルから (出力ピン, 出力インデックス) を探して接続する */
int32 ConnectLabelledOutputs(UMaterialEditorOnlyData* ED, const TArray<UMaterialExpression*>& Exprs)
{
    const TArray<FOutputPin> Pins = CodeMatGraph::GetMaterialOutputPins(ED);
    int32 Connected = 0;
    for (UMaterialExpression* E : Exprs)
    {
        TArray<FString> Parts;
        E->Desc.ParseIntoArray(Parts, TEXT(" / "));
        for (FString Part : Parts)
        {
            if (!Part.RemoveFromStart(TEXT("-> "))) continue;

            int32 OutIdx = 0;
            int32 Open = INDEX_NONE;
            if (Part.EndsWith(TEXT(")")) && Part.FindLastChar(TEXT('('), Open))
            {
                const FString OutName = Part.Mid(Open + 1, Part.Len() - Open - 2);
                Part = Part.Left(Open).TrimEnd();
                if (OutName.IsNumeric())
                {
                    OutIdx = FCString::Atoi(*OutName);
                }
                else
                {
                    const TArray<FExpressionOutput>& Outs = E->GetOutputs();
                    for (int32 i = 0; i < Outs.Num(); ++i)
                        if (Outs[i].OutputName.ToString() == OutName) { OutIdx = i; break; }
                }
            }

            for (const FOutputPin& Pin : Pins)
            {
                if (CodeMatGraph::GetOutputPinDisplayName(Pin.Value) == Part)
                {
                    Pin.Key->Expression  = E;
                    Pin.Key->OutputIndex = OutIdx;
                    ++Connected;
                }
            }
        }
    }
    return Connected;
}

TArray<UMaterialExpression*> GetNonCommentExpressions(UMaterialEditorOnlyData* ED)
{
    TArray<UMaterialExpression*> Exprs;
    for (UMaterialExpression* E : ED->ExpressionCollection.Expressions)
        if (E && !E->IsA<UMaterialExpressionComment>()) Exprs.Add(E);
    return Exprs;
}

} // namespace

// ============================================================================
// Public API
// ============================================================================

bool CodeMat::ExportPythonScript(const FCodeMatBuildSettings& S, FString& OutScript, FString& OutError, FString* OutLog)
{
    OutScript.Empty();
    OutError.Empty();
    FString Log;
    UMaterial* Mat = BuildTransientMaterial(S, Log);
    if (OutLog) *OutLog = Log;
    if (!Mat) { OutError = TEXT("Failed to create transient material"); return false; }

    UMaterialEditorOnlyData* ED = Mat->GetEditorOnlyData();
    const TArray<UMaterialExpression*> Exprs = GetNonCommentExpressions(ED);
    const TCHAR* ModeName = (S.OutputMode == ECodeMatOutputMode::MaterialNodes) ? TEXT("Material Nodes") : TEXT("Custom Node");

    FString P;
    P += TEXT("# -*- coding: utf-8 -*-\n");
    P += FString::Printf(TEXT("# Generated by furcraeaHLSLEditor from CodeMaterialAsset \"%s\"  (mode: %s)\n"), *S.AssetName, ModeName);
    P += TEXT("# Run in Unreal Editor: Output Log > Python, or Tools > Execute Python Script.\n");
    P += TEXT("# Requires the \"Python Editor Script Plugin\".\n");
    P += TEXT("import unreal\n\n");
    P += TEXT("MEL = unreal.MaterialEditingLibrary\n");
    P += FString::Printf(TEXT("ASSET_DIR  = %s\n"), *PyQuote(S.AssetFolder.IsEmpty() ? TEXT("/Game") : S.AssetFolder));
    P += FString::Printf(TEXT("ASSET_NAME = %s\n\n"), *PyQuote(FString::Printf(TEXT("M_%s_Py"), *S.AssetName)));
    P += PythonPrelude;
    AppendMaterialSettings(P, Mat);
    AppendNodes(P, Exprs);
    AppendConnections(P, ED, Exprs);
    P += TEXT("\nMEL.recompile_material(mat)\n");
    P += TEXT("unreal.EditorAssetLibrary.save_loaded_asset(mat)\n");
    P += TEXT("print(\"[furcraeaHLSL] generated: \" + mat.get_path_name())\n");

    Mat->MarkAsGarbage();
    OutScript = P;
    return true;
}

bool CodeMat::ExportT3D(const FCodeMatBuildSettings& S, FString& OutText, FString& OutError, FString* OutLog)
{
    OutText.Empty();
    OutError.Empty();
    FString Log;
    UMaterial* Mat = BuildTransientMaterial(S, Log);
    if (OutLog) *OutLog = Log;
    if (!Mat) { OutError = TEXT("Failed to create transient material"); return false; }

    UMaterialEditorOnlyData* ED = Mat->GetEditorOnlyData();
    for (const FOutputPin& Pin : CodeMatGraph::GetMaterialOutputPins(ED))
    {
        UMaterialExpression* E = Pin.Key->Expression;
        if (!E) continue;
        const FString Label = MakeOutputLabel(E, Pin);
        E->Desc = E->Desc.IsEmpty() ? Label : E->Desc + TEXT(" / ") + Label;
        E->bCommentBubbleVisible = true;
    }

    // マテリアルエディタのコピーと同じ手順でノードを書き出す
    UMaterialGraph* Graph = CodeMatGraph::GetOrCreateMaterialGraph(Mat);
    TSet<UObject*> Nodes;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node && Node->CanDuplicateNode())
        {
            Node->PrepareForCopying();
            Nodes.Add(Node);
        }
    }
    FEdGraphUtilities::ExportNodesToText(Nodes, OutText);
    for (UObject* Obj : Nodes)
        if (UMaterialGraphNode* MN = Cast<UMaterialGraphNode>(Obj)) MN->PostCopyNode();

    Mat->MaterialGraph = nullptr;
    Graph->MarkAsGarbage();
    Mat->MarkAsGarbage();

    if (OutText.IsEmpty()) { OutError = TEXT("No nodes were exported"); return false; }
    return true;
}

bool CodeMat::PasteT3DIntoMaterial(UMaterial* Material, const FString& T3DText, bool bConnectOutputs, FString& OutLog)
{
    OutLog.Empty();
    if (!Material) { OutLog = TEXT("Material is null"); return false; }

    UMaterialGraph* Graph = CodeMatGraph::GetOrCreateMaterialGraph(Material);
    if (!FEdGraphUtilities::CanImportNodesFromText(Graph, T3DText))
    {
        OutLog = TEXT("Text is not valid material node T3D");
        return false;
    }

    Material->PreEditChange(nullptr);
    Material->Modify();

    // FMaterialEditor::PasteNodesHereFromBuffer と同じ後処理
    TSet<UEdGraphNode*> Pasted;
    FEdGraphUtilities::ImportNodesFromText(Graph, T3DText, Pasted);

    TArray<UMaterialExpression*> NewExpressions;
    for (UEdGraphNode* Node : Pasted)
    {
        if (UMaterialGraphNode* GN = Cast<UMaterialGraphNode>(Node))
        {
            if (UMaterialExpression* E = GN->MaterialExpression)
            {
                E->Material = Material;
                E->Function = nullptr;
                if (E->HasAParameterName()) E->ValidateParameterName();
                Material->GetExpressionCollection().AddExpression(E);
                NewExpressions.Add(E);
            }
        }
        Node->CreateNewGuid();
    }
    for (UMaterialExpression* E : NewExpressions) E->PostCopyNode(NewExpressions);
    Graph->LinkMaterialExpressionsFromGraph();

    int32 Connected = 0;
    if (bConnectOutputs)
    {
        Connected = ConnectLabelledOutputs(Material->GetEditorOnlyData(), NewExpressions);
        Graph->RebuildGraph();
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();
    OutLog = FString::Printf(TEXT("pasted %d nodes, connected %d output pin(s)"), NewExpressions.Num(), Connected);
    return NewExpressions.Num() > 0;
}

#endif // WITH_EDITOR
