#
# morse-shield_nrt: arduino_morse_runner_20230912a.diff
# All rights reserved. Copyright (C) 2020,2023 by NARITA Tomio <nrt@ff.iij4u.or.jp> (JG1WYW)
# ABSOLUTELY NO WARRANTY
# 

■ これは何?

  中茂さんの Morse Shield [1]で動作するスケッチ(プログラム)の修正版で、
  モールス信号を送るための「エレクトリックキーヤー」の機能に、主な追加機能として、
  コールサインを『聞いて』、実際にキーを『打つ』練習ができる、『Morse Runner』風の機能を
  加えたものです。

■ 使い方

  - 設定
    * 「Sw4長押し」でメニューに入ります。
    * 「Sw1」で次の項目、「Sw1を長押し」で前の項目に移動
    * 「Sw2」 と「Sw3」で値の変更
    * 「Sw4」でメニューから出ます。

    << 追加項目 >>
    * "ToneFreq"  : トーンの周波数 [Hz] を選択                           [デフォルト 220] (範囲 220 - 1661)
    * "Word Spc"  : メモリーキーヤー送出時の Word 間の space (dotの個数) [デフォルト 4]   (範囲 1 - 10)
    * "Char Spc"  : メモリーキーヤー送出時の 文字間の space (dot個数)    [デフォルト 3]   (範囲 1 - 10)
    * "OffDelay"  : Off-delay timer の on / off                          [デフォルト on] (範囲 off / on)
    * "OffDlyDot" : Dot の off-delay (dot 1個分に対するパーセント)       [デフォルト 0]  (範囲 0 - 100)
                    Dot を離した後も、この時間、Dot が持続する
    * "OffDlyDsh" : Dash の off-delay (dot 1個分に対するパーセント)      [デフォルト 50] (範囲 0 - 200)
                    Dash を離した後も、この時間、Dash が持続する
    * "LAMemory"  : Look ahead memory の on / off                        [デフォルト on] (範囲 off / on)
    * "LAMratio"  : 先読みする割合 (dot 1個分に対するパーセント)         [デフォルト 25]  (範囲 0 - 100)
                    次の Dot / Dash の判定タイミングから、この時間だけ先読み
    * "AvodChtr"  : チャタリング防止の on / off                          [デフォルト on] (範囲 off / on)
    * "StbleCnt"  : 安定化のカウント                                     [デフォルト 50]  (範囲 5 - 100)
                    この回数、安定すれば key down/up の値を取り込む
    * "AtoKyChg"  : キー種別の自動選択 on / off                          [デフォルト off] (範囲 off / on)
                    Dash が3秒程度、入り続けると Straight Key と判定
    * "AtoKyEle"  : 自動選択するエレキーの種別                           [デフォルト IbcA](範囲 IbcA / IbcB / Supr)
                    自動判定後、Dash が終わった後、このエレキーに戻る

    << デフォルト値 >>
    * Lang A/ア   : ABC
    * Pdl Rev.    : off
    * Key mode    : IbcA  (* Iambic A を変更して動作確認しています)
    * Wait 1:*    : 3.00
    * SideTone    : on
    * CQint:ms    : 2000
    * CQ times    : 10

    (追加分)
    * "ToneFreq"  : 220
    * "Word Spc"  : 4
    * "Char Spc"  : 3
    * "OffDelay"  : on
    * "OffDlyDot" : 50
    * "OffDlyDsh" : 150
    * "LAMemory"  : on
    * "LAMratio"  : 20
    * "AvodChtr"  : on
    * "StbleCnt"  : 20
    * "AtoKyChg"  : on
    * "AtoKyEle"  : IbcA  (* Iambic A を変更して動作確認しています)

  - EEPROMへの設定の書き込み
    * 「Sw3長押し」で設定を EEPROM へ書き込みます。
      書き込んだデータは、バージョンが変更された際に書き込み対象のデータに追加変更があると、
      起動時に読み込まれず、各パラメータの値はリセットされた状態となります。

  - EEPROMから設定を読み込まずに起動
    * 「Sw4長押し」したたま電源を投入すると、設定を EEPROM から読み込まずに起動し、
       各パラメータの値はデフォルト値にリセットされた状態となります。

  - Morse Runner

    さぁ、CQ を出す練習をしましょう。パドルと Morse Shield があれば、無線機がなくても大丈夫。
    あなたの机の上がアマチュア無線のバーチャル空間に早変りです。

    * まず、エレキーのスピードを合わせましょう。Morse Shield のスピードつまみを回して、
      自分に合ったスピード、例えば 20wpm くらいに合わせましょう。

    * 次に「Sw2長押し」で Training メニューに入り、「Sw3」で Morse Runner を開始します。
      (Sw2長押しで、従来、Training メニューの表示で c だった部分が r に変更されている状態です。)
    
      - あなたが CQ を出した後、誰かがあなたを呼んできました。相手のコールサインは取れましたか?
        取れなかった場合は、取れたコールサインの途中まで打って、最後に "?" と打ちましょう。

      - ほら、あなたを呼んでますね。コールサインは取れましたか。何度でも "?" を打って聞き直しましょう。

      - さて、相手のコールサインが分かりましたか。では、相手のコールサインを打ってみましょう。

      - 間違えると、ブッとブザーが鳴ります。最初から打ち直してください。
        打ったコールサインはLCDパネルに表示されています。きちんと認識されていますか?
        文字と文字が繋っていると認識されませんので、少し丁寧に打ちましょう。

      - コールサインが合っていれば、ピロッとブザーが鳴って、次の局があなたを呼んできます。

      - 好きなだけ練習ができますが、Morse Runner に飽きたら、CW インベーダーで遊びましょう。
        一旦、「Sw2」を押して通常モードに戻った後、もう一度「Sw2」を押して Training メニューに入り、
        「Sw4」でインベーダーが始まります。(インベーダーに少し手を加えブザーが鳴るように修正してあります。)

      - もし、もっと本格的なコンテストの練習がしたければ、パソコンで使える Morse Runner [2]というソフトが
        ありますので、ぜひ、そちらもトライしてみてください。

■ 利用許諾

  - morse-shield_nrt.ino は、中茂さんが公開されている morse-shield_191220.ino を
    ベースに修正したもので、差分はアーカイブに含まれる arduino_morse_runner_(バー
    ジョン).diff です。オリジナルのファイルにこのパッチを当てることで差分の含まれる
    ファイルを生成することができます。

  オリジナルの利用許諾に従ってください。

■ 参照

  [1] モールス信号シールドArduino UNO用
      http://nksg.net/usbif4cw/lab/morse-shield/

  [2] Morse Runner - DX Atlas: Amateur Radio software
      http://www.dxatlas.com/MorseRunner/

---
