// ==========================================
// コンフィギュレーション設定 (PIC16F1827)
// ==========================================
// CONFIG1
#pragma config FOSC = INTOSC    // 内部クロック使用
#pragma config WDTE = OFF       // ウォッチドッグタイマ無効
#pragma config PWRTE = OFF      // パワーアップタイマ無効
#pragma config MCLRE = ON       // MCLRピン有効 (4ピン)
#pragma config CP = OFF         // コードプロテクト無効
#pragma config BOREN = OFF      // ブラウンアウトリセット無効
#pragma config CLKOUTEN = OFF   // クロック出力無効
#pragma config IESO = OFF       // 2速スタートアップ無効
#pragma config FCMEN = OFF      // フェイルセーフクロック監視無効

// CONFIG2
#pragma config WRT = OFF        // フラッシュメモリ書込保護無効
#pragma config PLLEN = OFF      // 4xPLL無効
#pragma config STVREN = ON      // スタックオーバーフローリセット有効
#pragma config BORV = LO        // ボルテージリセット低電圧
#pragma config LVP = ON         // 低電圧プログラミング有効 (ICSP用)

#include <xc.h>
#include <stdint.h>

#define _XTAL_FREQ 8000000      // 動作周波数 8MHz

// ==========================================
// 10セグメントLED制御関数
// ==========================================

// 10ビットのビットマスクで直接LEDを点滅制御する関数（アニメーション用）
void set_raw_leds(uint16_t mask) {
    // LED1〜LED8 (RB0〜RB7)
    LATB = (uint8_t)(mask & 0x00FF);

    // LED9 (RA6), LED10 (RA7)
    LATAbits.LATA6 = (mask & (1 << 8)) ? 1 : 0;
    LATAbits.LATA7 = (mask & (1 << 9)) ? 1 : 0;
}

// レベル表示用 (level: 0〜10本 点灯)
void set_bargraph(uint8_t level) {
    if (level > 10) level = 10;

    uint16_t pattern = 0;
    for (uint8_t i = 0; i < level; i++) {
        pattern |= (uint16_t)(1 << i);
    }
    set_raw_leds(pattern);
}

// ==========================================
// 起動アニメーション（セグメント動作テスト兼）
// ==========================================
void startup_animation(void) {
    // 1. 下から上へバーが伸びる
    for (uint8_t i = 1; i <= 10; i++) {
        set_bargraph(i);
        __delay_ms(40);
    }
    __delay_ms(150);

    // 2. 上から下へバーが縮む
    for (int8_t i = 9; i >= 0; i--) {
        set_bargraph((uint8_t)i);
        __delay_ms(40);
    }
    __delay_ms(100);

    // 3. 全点灯フラッシュ（2回点滅）
    for (uint8_t i = 0; i < 2; i++) {
        set_raw_leds(0x03FF); // 10本全点灯
        __delay_ms(80);
        set_raw_leds(0x0000); // 全消灯
        __delay_ms(80);
    }
    __delay_ms(200);
}

// ==========================================
// ADC（A/Dコンバータ）制御
// ==========================================
void ADC_Init(void) {
    // RA0(AN0: MCP9700A), RA1(AN1: サーミスタ) をアナログ入力に設定
    ANSELAbits.ANSA0 = 1;
    ANSELAbits.ANSA1 = 1;
    TRISAbits.TRISA0 = 1;
    TRISAbits.TRISA1 = 1;

    // ADC動作設定
    ADCON1bits.ADFM = 1;        // 10ビット結果を右詰め
    ADCON1bits.ADCS = 0b101;    // 変換クロック: Fosc/16
    ADCON1bits.ADPREF = 0b00;   // 基準電圧: VDD (3.3V)
    ADCON0bits.ADON = 1;        // ADCモジュールON
}

uint16_t ADC_Read(uint8_t channel) {
    ADCON0bits.CHS = channel;   // チャンネル選択 (0: AN0, 1: AN1)
    __delay_us(25);             // サンプリング容量チャージ待機
    
    ADCON0bits.GO = 1;          // A/D変換開始
    while (ADCON0bits.GO_nDONE); // 変換完了待ち
    
    return (uint16_t)((ADRESH << 8) | ADRESL);
}

// ==========================================
// メインルーチン
// ==========================================
void main(void) {
    // 内部オシレータを 8MHz に設定
    OSCCON = 0b01110000;

    // ポート初期設定
    ANSELA = 0x00;              // 一旦全デジタル
    ANSELB = 0x00;              // PORTB全デジタル
    
    // RA2 (1ピン: SW2 モード切替) を入力に設定
    TRISAbits.TRISA2 = 1;
    
    // LED制御ピンを出力・初期LOWに設定
    TRISAbits.TRISA6 = 0;       // LED9 (15ピン)
    TRISAbits.TRISA7 = 0;       // LED10 (16ピン)
    LATAbits.LATA6 = 0;
    LATAbits.LATA7 = 0;

    TRISB = 0x00;               // LED1〜LED8 (RB0〜RB7)
    LATB = 0x00;

    // ADC初期設定
    ADC_Init();

    // 起動アニメーション実行
    startup_animation();

    // メインループ
    while (1) {
        uint8_t mode = PORTAbits.RA2; // SW2 (1: 室温モード, 0: 水温モード)
        uint8_t bar_level = 0;

        if (mode == 1) {
            // ----------------------------------------------------
            // 【室温測定モード】(AN0: MCP9700A)
            // ----------------------------------------------------
            // 15℃(約0.65V / ADC≈202) 〜 35℃(約0.85V / ADC≈264) を10分割
            uint16_t val = ADC_Read(0b00000); // AN0 読み取り
            
            if (val < 200) {
                bar_level = 1;
            } else if (val >= 265) {
                bar_level = 10;
            } else {
                bar_level = (uint8_t)((val - 200) / 7) + 1;
            }
        } else {
            // ----------------------------------------------------
            // 【水温測定モード】(AN1: サーミスタ 0℃〜100℃)
            // ----------------------------------------------------
            // 温度が高いほどサーミスタ抵抗が下がり、ADC値が小さくなる特性
            uint16_t val = ADC_Read(0b00001); // AN1 読み取り

            if (val > 840) {
                bar_level = 0;   // 0℃未満 (消灯)
            } else if (val > 760) {
                bar_level = 1;   // 0〜10℃
            } else if (val > 660) {
                bar_level = 2;   // 10〜20℃
            } else if (val > 550) {
                bar_level = 3;   // 20〜30℃
            } else if (val > 450) {
                bar_level = 4;   // 30〜40℃
            } else if (val > 350) {
                bar_level = 5;   // 40〜50℃
            } else if (val > 270) {
                bar_level = 6;   // 50〜60℃
            } else if (val > 200) {
                bar_level = 7;   // 60〜70℃
            } else if (val > 150) {
                bar_level = 8;   // 70〜80℃
            } else if (val > 120) {
                bar_level = 9;   // 80〜90℃
            } else {
                bar_level = 10;  // 90〜100℃以上 (全点灯)
            }
        }

        // バーグラフ更新
        set_bargraph(bar_level);

        // 更新周期ウェイト
        __delay_ms(100);
    }
}
