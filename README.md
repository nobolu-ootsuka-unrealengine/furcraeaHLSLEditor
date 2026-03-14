# furcraeaHLSLEditor
[UE5.7][C++]遂に完成したC++プラグインUE用HLSL.EditorのMaterial作成機能 もうこれでノードスパゲッティから解放される


導入方法

０，以下のリンクをダウンロード

furcraeaHLSLEditor_UE5.7.4V6.zip
https://drive.google.com/file/d/1APYn65u1yo3xkJlDFmqJ2yuCE5zHViqc/view?usp=sharing

furcraeaHLSLEditor_UE5.7.4V5.zip
https://drive.google.com/file/d/1YiUHmR1g6d9WwvAtd1rGADsiNZEXRMNX/view?usp=sharing

furcraeaHLSLEditor_UE5.7.4.zip (V4)
https://drive.google.com/file/d/1gInP7W5hFB3pwlwbrbkekSzC8xOwusap/view?usp=sharing

furcraeaHLSLEditor_UE5.7.3V4.zip
https://drive.google.com/file/d/1gInP7W5hFB3pwlwbrbkekSzC8xOwusap/view?usp=sharing

furcraeaHLSLEditor_UE5.7.3V4.zip
https://drive.google.com/file/d/1XhVZgV6FRiyGOOquqO5nJVFXbmg_8DA-/view?usp=sharing

furcraeaHLSLEditor_UE5.7.3V3.zip
https://drive.google.com/file/d/1Qmz5Y_uT0VGKkrk1wskvw2DRbnND6or4/view?usp=drive_link

furcraeaHLSLEditor_UE5.7.3V2.zip
https://drive.google.com/file/d/1CSXmSebk7hLM02qM5jKDv2ndNJ4jhUi7/view?usp=sharing


１，Pluginsフォルダにzipを解凍して入れてください。

　　パスの例：UEプロジェクトフォルダ\Plugins\furcraeaHLSLEditor\Shaders

２，プラグインで「furcraeaHLSLEditor」を有効化

使用方法
コンテンツブラウザで右クリック＞Miscellaneous＞Code Material Assetを作成


できた New Code Material AssetをダブルクリックでHLSL編集画面が開く


Saveをクリックすると。。。GeneratedフォルダーにM_NewCodeMaterialAssetが作成されます。



まだまだ、１つしかCodeMatUser.ushがないので一つしかマテリアルが成立しないなどの不備があるので更新していく予定です。<-以下の画像のように修正済みです。


furcraeaHLSLEditorをGitHub で公開しました。
https://github.com/nobolu-ootsuka-unrealengine/furcraeaHLSLEditor/commits?author=nobolu-ootsuka-unrealengine



修正履歴



１つしかCodeMatUser.ushがないので一つしかマテリアルが成立しないなどの不備を修正

furcraeaHLSLEditor_UE5.7.3V2

Projects と RenderCore を bBuildEditorをガード外に移動（ランタイムでも必要なモジュールのため）

furcraeaHLSLEditor_UE5.7.3V3

CodeMaterialAsset が吐くMaterialのフォルダを (固定)Generated から (ユーザーが作った)CodeMaterialAssetと同じフォルダに変更した

CodeMaterialCompiler.cpp に2箇所変更しました。

① MakeMaterialPackagePath 関数（生成先パスを決定）

変更前：/Game/Generated/M_xxx 固定
変更後：Asset->GetOutermost()->GetName() でアセット自身のパッケージパスを取得し、FPackageName::GetLongPackagePath()
でフォルダ部分を抜き出して {同じフォルダ}/M_xxx を返す ② CreateOrLoadMaterial の MakeDirectory 呼び出し
変更前：MakeDirectory(“/Game/Generated”) 固定
変更後：MakeDirectory(FPackageName::GetLongPackagePath(MakeMaterialPackagePath(Asset))) —
生成先パスからフォルダを算出して作成 これにより、例えば Content/0_furcraeaTokyo/CodeMaterialAsset/NewCodeMaterialAsset に保存された CodeMaterialAsset
に対しては、生成マテリアルも同じ Content/0_furcraeaTokyo/CodeMaterialAsset/M_NewCodeMaterialAsset に出力されます。


furcraeaHLSLEditor_UE5.7.4V5.zip
ルール: FragmentShaderCode / VertexShaderCode には 関数定義を書かない。ヘルパーが必要なら全てインライン化する。
コンパイラの整理  
- ExtractHelperFunctions + エスケープトリック (将来の一般的ヘルパー対応) は保持
// @param float3 ColorC = 0.90,0.20,1.20//でBPでパラメーター化

furcraeaHLSLEditor_UE5.7.4V6.zip
・日本語コメントがあるとVertex Shader側のコードカラーリングが効いてない不具合を修正
・こんどは両ペインで改行が入力できないを解決
・ユーザーが .ush のボディ内で Phase * Time のように Time を直接書いても View.GameTime として展開され、HLSL
  コンパイルエラー → クラッシュが防がれます。