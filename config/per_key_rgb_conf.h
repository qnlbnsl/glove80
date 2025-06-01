//////////////////////////////////////////////////////////////////////////////
//
// Per-key RGB indicators for layers & mods -- requires PR36 beta firmware:
// override "EXPERIMENTAL_RGB_LAYER" to "y" in Advanced Configuration and
// select "community.pr36.per-key-rgb" from the drop-down menu located at
// Glove80 Layout Editor > Settings > Advanced Settings > Firmware Version
//
// - https://github.com/moergo-sc/zmk/pull/36
// - https://github.com/moergo-sc/zmk/pull/30
// - https://github.com/zmkfirmware/zmk/pull/2752
// - https://dev.glove80.com/#/layout/user/c42ac32d-dc0d-4160-9ee3-fd0fc15f1725
//
//////////////////////////////////////////////////////////////////////////////
#if __has_include(<dt-bindings/zmk/rgb_colors.h>)  
  // ==== PER-KEY-RGB <section begins> ====
  #include <dt-bindings/zmk/rgb_colors.h>
  / {
    underglow-layer {
      compatible  = "zmk,underglow-layer";

      //
      // color definitions from QMK
      // https://github.com/qmk/qmk_firmware/blob/master/quantum/color.h
      //
      #define RGB_AZURE     0x99F5FF // #99F5FF
      #define RGB_BLACK     0x000000 // #000000
      #define RGB_BLUE      0x0000FF // #0000FF
      #define RGB_CHARTRSE  0x80FF00 // #80FF00
      #define RGB_CORAL     0xFF7C4D // #FF7C4D
      #define RGB_CYAN      0x00FFFF // #00FFFF
      #define RGB_GOLD      0xFFD900 // #FFD900
      #define RGB_GOLDNROD  0xD9A521 // #D9A521
      #define RGB_GREEN     0x00FF00 // #00FF00
      #define RGB_MAGENTA   0xFF00FF // #FF00FF
      #define RGB_ORANGE    0xFF8000 // #FF8000
      #define RGB_PINK      0xFF80BF // #FF80BF
      #define RGB_PURPLE    0x7A00FF // #7A00FF
      #define RGB_RED       0xFF0000 // #FF0000
      #define RGB_SPRINGRN  0x00FF80 // #00FF80
      #define RGB_TEAL      0x008080 // #008080
      #define RGB_TURQUOIS  0x476E6A // #476E6A
      #define RGB_WHITE     0xFFFFFF // #FFFFFF
      #define RGB_YELLOW    0xFFFF00 // #FFFF00

      //
      // color definitions from MoErgo
      // https://github.com/moergo-sc/zmk/blob/aca523dfce9b6447ffd19d833b791d44f1f286de/app/src/rgb_underglow.c#L259-L265
      //
      #define RGB_DULL_GRN  0x00ff68 // #00ff68
      #define RGB_LILAC     0x6b1fce // #6b1fce

      //
      // underglow for the colors above
      //
      #define AZURE     &ug RGB_AZURE
      #define BLACK     &ug RGB_BLACK
      #define BLUE      &ug RGB_BLUE
      #define CHARTRSE  &ug RGB_CHARTRSE
      #define CORAL     &ug RGB_CORAL
      #define CYAN      &ug RGB_CYAN
      #define GOLD      &ug RGB_GOLD
      #define GOLDNROD  &ug RGB_GOLDNROD
      #define GREEN     &ug RGB_GREEN
      #define MAGENTA   &ug RGB_MAGENTA
      #define ORANGE    &ug RGB_ORANGE
      #define PINK      &ug RGB_PINK
      #define PURPLE    &ug RGB_PURPLE
      #define RED       &ug RGB_RED
      #define SPRINGRN  &ug RGB_SPRINGRN
      #define TEAL      &ug RGB_TEAL
      #define TURQUOIS  &ug RGB_TURQUOIS
      #define WHITE     &ug RGB_WHITE
      #define YELLOW    &ug RGB_YELLOW
      #define ________  &ug RGB_BLACK
      #define DULL_GRN  &ug RGB_DULL_GRN
      #define LILAC     &ug RGB_LILAC

      //
      // colors for mouse speed scaling in Mouse layers
      //
      #define MSE_FAST GOLD
      #define MSE_WARP CHARTRSE
      #define MSE_SLOW CORAL

      //
      // underglow for keyboard locks on the Base layer
      //
      #define UGSL_BAS &ug_sl RGB_LILAC RGB_ORANGE // ScrollLock COLOR_OFF COLOR_ON
      #define UGNL_BAS &ug_nl RGB_LILAC RGB_ORANGE // NumLock    COLOR_OFF COLOR_ON
      #define UGCL_BAS &ug_cl RGB_BLACK RGB_ORANGE // CapsLock   COLOR_OFF COLOR_ON

      //
      // underglow for keyboard locks on the System layer
      //
      #define UGSL_SYS &ug_sl RGB_BLACK RGB_ORANGE // ScrollLock COLOR_OFF COLOR_ON
      #define UGNL_SYS &ug_nl RGB_BLACK RGB_ORANGE // NumLock    COLOR_OFF COLOR_ON
      #define UGCL_SYS &ug_cl RGB_BLACK RGB_ORANGE // CapsLock   COLOR_OFF COLOR_ON

      /*
      Template {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________ ________ ________ ________  ________ ________ ________ ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________          ________ ________ ________  ________ ________ ________          ________ ________ ________ ________ ________
        >;
        layer-id = <LAYER_Template>;
      };
      */

      BaseLayer {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ ________ ________ ________ ________ LILAC    LILAC    BLUE          BLUE    LILAC    LILAC ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ CORAL             UGSL_BAS UGNL_BAS UGCL_BAS  ________    LILAC    LILAC             CORAL ________ ________ ________ ________
        >;
        layer-id = <0>;
      };

      #ifdef LAYER_LeftPinky
      LeftPinky {
        bindings = <
          ________ MAGENTA  ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ ________ ________ ________ ________ LILAC    LILAC    BLUE          BLUE    LILAC    LILAC ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ CORAL             LILAC    LILAC    ________  ________    LILAC    LILAC             CORAL ________ ________ ________ ________
        >;
        layer-id = <LAYER_LeftPinky>;
      };
      #endif

      #ifdef LAYER_LeftRingy
      LeftRingy {
        bindings = <
          ________ ________ CYAN     ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ ________ ________ ________ ________ LILAC    LILAC    BLUE          BLUE    LILAC    LILAC ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ CORAL             LILAC    LILAC    ________  ________    LILAC    LILAC             CORAL ________ ________ ________ ________
        >;
        layer-id = <LAYER_LeftRingy>;
      };
      #endif

      #ifdef LAYER_LeftMiddy
      LeftMiddy {
        bindings = <
          ________ ________ ________ GREEN    ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ ________ ________ ________ ________ LILAC    LILAC    BLUE          BLUE    LILAC    LILAC ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ CORAL             LILAC    LILAC    ________  ________    LILAC    LILAC             CORAL ________ ________ ________ ________
        >;
        layer-id = <LAYER_LeftMiddy>;
      };
      #endif

      #ifdef LAYER_LeftIndex
      LeftIndex {
        bindings = <
          ________ ________ ________ ________ YELLOW                                                                            ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ ________ ________ ________ ________ LILAC    LILAC    BLUE          BLUE    LILAC    LILAC ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ CORAL             LILAC    LILAC    ________  ________    LILAC    LILAC             CORAL ________ ________ ________ ________
        >;
        layer-id = <LAYER_LeftIndex>;
      };
      #endif

      #ifdef LAYER_RightIndex
      RightIndex {
        bindings = <
          ________ ________ ________ ________ ________                                                                            YELLOW ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ ________ ________ ________ ________ LILAC    LILAC    BLUE          BLUE    LILAC    LILAC ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ CORAL             LILAC    LILAC    ________  ________    LILAC    LILAC             CORAL ________ ________ ________ ________
        >;
        layer-id = <LAYER_RightIndex>;
      };
      #endif

      #ifdef LAYER_RightMiddy
      RightMiddy {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________    GREEN ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ ________ ________ ________ ________ LILAC    LILAC    BLUE          BLUE    LILAC    LILAC ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ CORAL             LILAC    LILAC    ________  ________    LILAC    LILAC             CORAL ________ ________ ________ ________
        >;
        layer-id = <LAYER_RightMiddy>;
      };
      #endif

      #ifdef LAYER_RightRingy
      RightRingy {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________     CYAN ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ ________ ________ ________ ________ LILAC    LILAC    BLUE          BLUE    LILAC    LILAC ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ CORAL             LILAC    LILAC    ________  ________    LILAC    LILAC             CORAL ________ ________ ________ ________
        >;
        layer-id = <LAYER_RightRingy>;
      };
      #endif

      #ifdef LAYER_RightPinky
      RightPinky {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________  MAGENTA ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ ________ ________ ________ ________ LILAC    LILAC    BLUE          BLUE    LILAC    LILAC ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ CORAL             LILAC    LILAC    ________  ________    LILAC    LILAC             CORAL ________ ________ ________ ________
        >;
        layer-id = <LAYER_RightPinky>;
      };
      #endif

      #ifdef LAYER_Cursor
      Cursor {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ YELLOW   ________ ________ ________ ________                                                        ________ ________ ________ ________   YELLOW ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________    AZURE    AZURE    AZURE    AZURE ________
          ________ ________ ________ ________ ________ ________ ________ ________ RED       ________ ________ ________ ________     TEAL     TEAL     TEAL     TEAL ________
          ________ ________ ________ ________ ________          DULL_GRN ________ ________  ________ ________ ________          ________ ________ ________ ________ ________
        >;
        layer-id = <LAYER_Cursor>;
      };
      #endif

      #ifdef LAYER_Number
      Number {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ YELLOW   ________ ________ ________ ________                                                        ________ CHARTRSE CHARTRSE CHARTRSE ________ ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________ CHARTRSE CHARTRSE CHARTRSE ________ ________
          ________ ________ ________ ________ ________ ________ ________ ________ RED       ________ ________ ________ ________ CHARTRSE CHARTRSE CHARTRSE ________ ________
          ________ ________ ________ ________ ________          ________ DULL_GRN ________  ________ ________ CHARTRSE          ________ ________ ________ ________ ________
        >;
        layer-id = <LAYER_Number>;
      };
      #endif

      #ifdef LAYER_Function
      Function {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ YELLOW   ________ ________ ________ ________                                                        ________ PINK     PINK     PINK     ________ ________
          ________ MAGENTA  CYAN     GREEN    YELLOW   ________                                                        ________ PINK     PINK     PINK     ________ ________
          ________ ________ ________ ________ ________ ________ DULL_GRN ________ RED       ________ ________ ________ ________ PINK     PINK     PINK     ________ ________
          ________ ________ ________ ________ ________          ________ ________ ________  ________ ________ ________          ________ ________ ________ ________ ________
        >;
        layer-id = <LAYER_Function>;
      };
      #endif

      #ifdef LAYER_Emoji
      Emoji {
        bindings = <
          ________ ________ ________ ________ ________                                                                              CYAN ________ ________     CYAN ________
          ________ ________ ________ ________ ________ ________                                                        ________     CYAN ________ ________     CYAN ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                            CYAN ________ ________ ________ ________     CYAN
          ________ ________ ________ ________ ________ ________ YELLOW   DULL_GRN RED       ________ ________ ________ ________     CYAN ________ ________     CYAN ________
          ________ ________ ________ ________ ________          ________ ________ ________  ________ ________ ________          ________     CYAN     CYAN ________ ________
        >;
        layer-id = <LAYER_Emoji>;
      };
      #endif

      #ifdef LAYER_Symbol
      Symbol {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ ________ ________ ________ ________                                                        ________    AZURE    AZURE    AZURE    AZURE     TEAL
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________     TEAL
          ________ ________ ________ ________ ________ ________ ORANGE   ________ ________       RED ________ ________ ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________          ________ ________ ________  ________ ________ DULL_GRN          ________ ________ ________ ________ ________
        >;
        layer-id = <LAYER_Symbol>;
      };
      #endif

      #ifdef LAYER_Mouse
      Mouse {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ TEAL     AZURE    TEAL     ________                                                        ________     TEAL     TEAL     TEAL     TEAL ________
          ________ TEAL     AZURE    AZURE    AZURE    ________                                                        ________ MSE_FAST MSE_WARP MSE_SLOW ________ ________
          ________ TEAL     MSE_SLOW MSE_WARP MSE_FAST ________ MAGENTA  ________ PINK           RED ________ ________ ________    AZURE    AZURE    AZURE    AZURE ________
          ________ ________ ________ ________ ________          YELLOW   ORANGE   PINK      ________ DULL_GRN ________            YELLOW   ORANGE  MAGENTA     PINK     PINK
        >;
        layer-id = <LAYER_Mouse>;
      };
      #endif

      #ifdef LAYER_MouseFast
      MouseFast {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ MSE_FAST MSE_FAST MSE_FAST ________                                                        ________ MSE_FAST MSE_FAST MSE_FAST MSE_FAST ________
          ________ MSE_FAST MSE_FAST MSE_FAST MSE_FAST ________                                                        ________ MSE_FAST MSE_WARP MSE_SLOW ________ ________
          ________ MSE_FAST MSE_SLOW MSE_WARP MSE_FAST ________ MAGENTA  ________ PINK           RED ________ ________ ________ MSE_FAST MSE_FAST MSE_FAST MSE_FAST ________
          ________ ________ ________ ________ ________          YELLOW   ORANGE   PINK      ________ DULL_GRN ________            YELLOW   ORANGE  MAGENTA     PINK     PINK
        >;
        layer-id = <LAYER_MouseFast>;
      };
      #endif

      #ifdef LAYER_MouseWarp
      MouseWarp {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ MSE_WARP MSE_WARP MSE_WARP ________                                                        ________ MSE_WARP MSE_WARP MSE_WARP MSE_WARP ________
          ________ MSE_WARP MSE_WARP MSE_WARP MSE_WARP ________                                                        ________ MSE_FAST MSE_WARP MSE_SLOW ________ ________
          ________ MSE_WARP MSE_SLOW MSE_WARP MSE_FAST ________ MAGENTA  ________ PINK           RED ________ ________ ________ MSE_WARP MSE_WARP MSE_WARP MSE_WARP ________
          ________ ________ ________ ________ ________          YELLOW   ORANGE   PINK      ________ DULL_GRN ________            YELLOW   ORANGE  MAGENTA     PINK     PINK
        >;
        layer-id = <LAYER_MouseWarp>;
      };
      #endif

      #ifdef LAYER_MouseSlow
      MouseSlow {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ MSE_SLOW MSE_SLOW MSE_SLOW ________                                                        ________ MSE_SLOW MSE_SLOW MSE_SLOW MSE_SLOW ________
          ________ MSE_SLOW MSE_SLOW MSE_SLOW MSE_SLOW ________                                                        ________ MSE_FAST MSE_WARP MSE_SLOW ________ ________
          ________ MSE_SLOW MSE_SLOW MSE_WARP MSE_FAST ________ MAGENTA  ________ PINK           RED ________ ________ ________ MSE_SLOW MSE_SLOW MSE_SLOW MSE_SLOW ________
          ________ ________ ________ ________ ________          YELLOW   ORANGE   PINK      ________ DULL_GRN ________            YELLOW   ORANGE  MAGENTA     PINK     PINK
        >;
        layer-id = <LAYER_MouseSlow>;
      };
      #endif

      #ifdef LAYER_System
      System {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________   YELLOW    GREEN     CYAN  MAGENTA ________
          ________ ________ ________ ________ ORANGE   ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________   ORANGE ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________ ________ ________ ________       RED ________ DULL_GRN ________ UGSL_SYS UGNL_SYS UGCL_SYS ________ ________
          ________ ________ ________ ________ PINK              UGSL_SYS UGNL_SYS UGCL_SYS  ________ ________ ________          ________ ________ ________ ________ ________
        >;
        layer-id = <LAYER_System>;
      };
      #endif

      #ifdef LAYER_World
      World {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________     CYAN     CYAN ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________   YELLOW    GREEN    GREEN   YELLOW ________
          ________ ________ ________ ________ ________ ________ ________ ________ ________       RED DULL_GRN ________ ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________          ________ ________ ________  ________ ________ ________          ________ ________ ________ ________ ________
        >;
        layer-id = <LAYER_World>;
      };
      #endif

      #ifdef LAYER_Typing
      Typing {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          YELLOW   ________ ________ ________ ________ ________ ________ ________ RED            RED ________ ________ ________ ________ ________ ________ ________   YELLOW
          ________ ________ ________ ________ DULL_GRN          ________ ________ ________  ________ ________ ________          DULL_GRN ________ ________ ________ ________
        >;
        layer-id = <LAYER_Typing>;
      };
      #endif

      #ifdef LAYER_Gaming
      Gaming {
        bindings = <
          ORANGE   ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ AZURE    ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ YELLOW   AZURE    AZURE    AZURE    ________ YELLOW   CYAN     RED         ORANGE    AZURE ________ ________ ________ ________ ________ ________ ________
          ________ GREEN    ________ ________ ________          ________ GREEN    RED          AZURE    AZURE    AZURE          ________ ________ ________ ________ ________
        >;
        layer-id = <LAYER_Gaming>;
      };
      #endif

      #ifdef LAYER_Lower
      Lower {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ UGSL_SYS
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ UGNL_SYS
          ________ ________ ________ ________ ________ ________ LILAC    LILAC    RED            RED    LILAC    LILAC ________ ________ ________ ________ ________ ________
          ________ UGCL_SYS ________ ________ CORAL             LILAC    LILAC    CYAN          CYAN    LILAC    LILAC             CORAL ________ ________ ________ ________
        >;
        layer-id = <LAYER_Lower>;
      };
      #endif

      #ifdef LAYER_Factory
      Factory {
        bindings = <
          ________ ________ ________ ________ ________                                                                          ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________                                                        ________ ________ ________ ________ ________ ________
          ________ ________ ________ ________ ________ ________ ________ ________ RED       ________ ________ ________ ________ ________ ________ ________ ________ ________
          RED      ________ ________ ________ ________          ________ ________ ________  ________ ________ ________          ________ ________ ________ ________ ________
        >;
        layer-id = <LAYER_Factory>;
      };
      #endif

    };
  };
  // ==== PER-KEY-RGB <section ends> =====
#endif