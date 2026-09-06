# I-26 Raspberry Pi Pico Firmware

I-26（伊-26）用のRaspberry Pi Picoフライトコントローラ・ファームウェアです。

このリポジトリは、**GitHub上でプログラムを編集 → GitHub ActionsでRaspberry Pi Pico向けにビルド → 書き込み用UF2を取得**できるように構成しています。

同時に、実際のプログラム本体を `firmware/i26/` に完全に分離しています。このフォルダだけをZIP化して他人へ渡しても、Pico SDKを指定して単独ビルドできます。

## 構成

```text
i26-pico-firmware/
├─ firmware/
│  └─ i26/                    ← プログラム本体。ここだけ切り離して渡せる
│     ├─ CMakeLists.txt
│     ├─ README.md
│     ├─ pico_copter.cpp/.hpp
│     ├─ control.cpp/.hpp
│     ├─ ekf.cpp/.hpp
│     ├─ sensor.cpp/.hpp
│     ├─ pwm.cpp/.hpp
│     ├─ radio.cpp/.hpp
│     └─ lsm9ds1_reg.c/.h
├─ .github/workflows/
│  └─ build.yml               ← Pico専用クラウドビルド + Pages公開
├─ web/                       ← UF2配布ページ
└─ README.md
```

## GitHub上で編集する

通常のGitHub画面でキーボードの `.` キーを押すと `github.dev` が開きます。

主な編集対象は次です。

```text
firmware/i26/
```

編集後に `Commit & Push` すると、`main` ブランチではGitHub Actionsが自動でRaspberry Pi Pico向けUF2をビルドします。Actions画面から `Run workflow` で手動実行することもできます。

## 生成物

成功したビルドでは次をまとめたArtifactを生成します。

```text
i26-pico.uf2              ← Raspberry Pi Picoへの書き込み用
i26-pico.uf2.sha256
i26-firmware-source.zip   ← 他人へ渡しやすいプログラム本体ZIP
manifest.json
configure.log
build.log
```

## GitHub Pagesを有効にする

最初の1回だけ、リポジトリの `Settings` → `Pages` → `Build and deployment` で `Source` を **GitHub Actions** に設定してください。

その後は `main` のプログラムやビルド設定を更新するたびに、UF2のビルド成功後、GitHub Actionsが配布ページも自動更新します。配布ページには `i26-pico.uf2` と `i26-firmware-source.zip` の両方を置きます。

## Raspberry Pi Picoへの書き込み

1. `i26-pico.uf2` をダウンロードする
2. Picoの `BOOTSEL` ボタンを押したままUSB接続する
3. PCに表示された `RPI-RP2` ドライブへUF2をコピーする
4. コピー完了後、Picoが自動で再起動する

## プログラム本体だけを渡す

次のどちらかで渡せます。

- リポジトリの `firmware/i26/` フォルダをそのまま渡す
- Actions/Pagesで生成される `i26-firmware-source.zip` を渡す

受け取った側は、展開した `i26` フォルダ内で次のようにビルドできます。

```bash
cmake -S . -B build \
  -DPICO_SDK_PATH=/path/to/pico-sdk \
  -DPICO_BOARD=pico \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

生成物は `build/i26.uf2` です。

## 固定しているビルド条件

| 項目 | 設定 |
|---|---|
| 対象ボード | Raspberry Pi Pico (`pico`) のみ |
| MCU | RP2040 |
| Pico SDK | 2.3.0 |
| C++ | C++17 |
| Eigen | 3.4.0 / fixed commit `3147391d946bb4b6c68edd901f2add6ac1f31f8c` |

元プログラムにあった特定PC専用のEigenパスは削除し、他のPCやGitHub Actionsでも同じ構成でビルドできるようにしています。
