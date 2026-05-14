# furcraeaHLSLEditor
[UE5.7][C++]遂に完成したC++プラグインUE用HLSL.EditorのMaterial作成機能 もうこれでノードスパゲッティから解放される


導入方法

０，以下のリンクをダウンロード

furcraeaHLSLEditor_UE5.8.0V9.zip  UE5.8に対応しました。  
https://drive.google.com/file/d/14nWd_4X9uQvt6KjFlmgRbYp2LUYSth8o/view?usp=sharing

furcraeaHLSLEditor_UE5.7.4V8.zip　float4出力に対応しました。  
https://drive.google.com/file/d/1PaBHAT9O4m6DC6UceUtwrj5o8N9qrrhn/view?usp=sharing

furcraeaHLSLEditor_UE5.7.4V7.zip    ポストプロセスマテリアル対応  
https://drive.google.com/file/d/1ieBRb5zpW36LGldmREROGcWJ8WZot1Xg/view?usp=sharing

furcraeaHLSLEditor_UE5.7.4V6_2.zip　コーディングBugFix  
https://drive.google.com/file/d/1BzMiQt4dXcu8p-dIfkyH3v1SRVSKjiCn/view?usp=sharing

furcraeaHLSLEditor_UE5.7.4V6.zip　コーディングBugFix  
https://drive.google.com/file/d/1APYn65u1yo3xkJlDFmqJ2yuCE5zHViqc/view?usp=sharing

furcraeaHLSLEditor_UE5.7.4V5.zip     関数インライン化によるコンパイラ整理  
https://drive.google.com/file/d/1YiUHmR1g6d9WwvAtd1rGADsiNZEXRMNX/view?usp=sharing

furcraeaHLSLEditor_UE5.7.4.zip (V4)　FragmentShader とVertexShader(Optional)に分離  
https://drive.google.com/file/d/1gInP7W5hFB3pwlwbrbkekSzC8xOwusap/view?usp=sharing

furcraeaHLSLEditor_UE5.7.3V4.zip　ユーザーが作った)CodeMaterialAssetと同じフォルダに変更  
https://drive.google.com/file/d/1XhVZgV6FRiyGOOquqO5nJVFXbmg_8DA-/view?usp=sharing

furcraeaHLSLEditor_UE5.7.3V3.zip　Projects と RenderCore を bBuildEditorをガード外に移動  
https://drive.google.com/file/d/1Qmz5Y_uT0VGKkrk1wskvw2DRbnND6or4/view?usp=drive_link

furcraeaHLSLEditor_UE5.7.3V2.zip　１つしかCodeMatUser.ushがない不備修正  
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



修正履歴は以下をご覧ください。

https://furcraea.verse.jp/wp/2026/03/09/ue5-7c-finally-completed-the-material-creation-function-of-the-hlsl-editor-c-plugin-for-ue/
