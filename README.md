# Zyfamate

将棋の詰み探索（詰将棋）エンジン。df-pn+ による詰み探索を、盤面・指し手生成・make/unmake・証明駒まで含めて1ファイル（`source/zyfamate_core.h`）で実装し、USI の詰将棋エンジンとして動く。もとは作者の Python 実装（盤面・df-pn）を C へ移植・高速化したもの。

## 使い方（USI）
```
usi / isready
setoption name USI_Hash value <MB>
position startpos | sfen <sfen> [moves ...]
go mate <ms>        -> checkmate <手順> | checkmate nomate
stop / quit
```

## build
```
clang++ -std=c++20 -O3 -march=native -static source/main.cpp -o zyfamate.exe -lpthread
```
Windows（MSYS2 ucrt64 の clang、llvm-mingw）で確認。`-march=native` なので動かす機械で build すること。

## 他のエンジンへの組込み
`source/zyfamate_core.h` は名前空間 `Zyfamate` に閉じ、定義したマクロは末尾で全て `#undef` する。将棋エンジン [Cyfamate](https://github.com/yfamate/Cyfamate) は、このファイルを変更せずに借りて、常駐の詰み探索として通常探索と並走させている。組込み先がやねうら王系なら `ZYFAMATE_HOST_YANEURAOU` を定義すると、証明木を探索側の局面鍵（やねうら王の Zobrist）と指し手形式で渡す経路が有効になる。

## ライセンス
GNU GPL v3（`LICENSE`）。
