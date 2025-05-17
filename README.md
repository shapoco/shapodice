# ShapoDice (WIP)

8pin の 8bit マイコン ATtiny25/ATtiny45/ATtiny85 用の電子サイコロ

- Arduino + ATTinyCore 使用
- 8 個の LED でサイコロの目 (&#x2680;, &#x2681;, &#x2682;, &#x2683;, &#x2684;, &#x2685;) を表示
- &#x2680; は赤色で表示
- [xoroshiro128++](https://prng.di.unimi.it/xoroshiro128plusplus.c) による周期の長い乱数を使用した出目の決定
- 乱数生成器の状態変数を EEPROM に保持することで起動自の出目を予測不能にする
- PWM と圧電サウンダによる効果音
- セルフパワーオフ (主電源スイッチ不要)
- バッテリー残量の低下を LED で警告

## ライセンス

このプロジェクトのライセンスは未定です。

このプロジェクトでは乱数生成のために [David Blackman と Sebastiano Vigna](https://prng.di.unimi.it/) による
[xoroshiro128++](https://prng.di.unimi.it/xoroshiro128plusplus.c) (パブリックドメイン) を改変して使用しています。
