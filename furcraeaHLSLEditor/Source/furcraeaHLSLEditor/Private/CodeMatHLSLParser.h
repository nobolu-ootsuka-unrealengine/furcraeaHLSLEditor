// Plugins/furcraeaHLSLEditor/Source/furcraeaHLSLEditor/Private/CodeMatHLSLParser.h
//
// HLSL サブセットの AST と構文解析 API。
// コメント / # 行は除去されるが改行は保持されるため、エラーの行番号は元コードと一致する。

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

namespace CodeMatHLSL
{
// ============================================================================
// AST
// ============================================================================

struct FAst;
using FAstPtr = TSharedPtr<FAst>;

struct FAst
{
    enum class EKind : uint8 { Num, Ident, Unary, Binary, Ternary, Call, Member, Index };
    EKind   Kind = EKind::Num;
    FString S;            // 識別子 / 演算子 / 関数名 / メンバ名
    double  Num  = 0.0;
    int32   Line = 0;
    TArray<FAstPtr> A;    // オペランド / 引数
    FAstPtr Obj;          // Member / メソッド呼び出しの対象
};

struct FStmt;
using FStmtPtr = TSharedPtr<FStmt>;

struct FStmt
{
    enum class EKind : uint8 { Empty, Decl, Assign, Return, ExprS, Block, If, For };
    EKind   Kind = EKind::Empty;
    int32   Line = 0;

    FString Type;                                // Decl
    TArray<TPair<FString, FAstPtr>> Decls;       // Decl: 名前と初期化式

    FAstPtr Target;                              // Assign
    FString Op;                                  // Assign: "=", "+=" ...
    FAstPtr Value;                               // Assign 右辺 / Return / ExprS / If 条件 / For 条件

    TArray<FStmtPtr> Body;                       // Block / For 本体 (Body[0])
    FStmtPtr Then;                               // If then / For init
    FStmtPtr Else;                               // If else / For step
};

struct FParam
{
    FString Type;
    FString Name;
    bool    bOut = false;
};

struct FFunc
{
    FString RetType;
    FString Name;
    TArray<FParam>   Params;
    TArray<FStmtPtr> Body;
};

// ============================================================================
// API
// ============================================================================

/** ソース全体を解析して関数定義とグローバル定数宣言を返す */
bool ParseProgram(const FString& Source, TArray<FFunc>& OutFuncs, TArray<FStmtPtr>& OutGlobals, FString& OutError);

/** "float3" → 3, "float" → 1, 行列 → -1, 型名でなければ 0 */
int32 TypeComponents(const FString& TypeName);
bool  IsTextureType(const FString& TypeName);
bool  IsSamplerType(const FString& TypeName);
bool  IsIntType(const FString& TypeName);
bool  IsTypeName(const FString& TypeName);

} // namespace CodeMatHLSL

#endif // WITH_EDITOR
