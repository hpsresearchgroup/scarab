/* Copyright 2020 HPS/SAFARI Research Groups
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

struct TAGE_SC_L_CONFIG_64KB {
  // static constexpr bool PIPELINE_SUPPORT = true;
  static constexpr bool USE_LOOP_PREDICTOR       = true;
  static constexpr bool USE_SC                   = true;
  static constexpr int  CONFIDENCE_COUNTER_WIDTH = 7;

  struct TAGE {
    static constexpr int MIN_HISTORY_SIZE            = 6;
    static constexpr int MAX_HISTORY_SIZE            = 3000;
    static constexpr int NUM_HISTORIES               = 18;
    static constexpr int PATH_HISTORY_WIDTH          = 27;
    static constexpr int FIRST_LONG_HISTORY_TABLE    = 13;
    static constexpr int FIRST_2WAY_TABLE            = 9;
    static constexpr int LAST_2WAY_TABLE             = 22;
    static constexpr int SHORT_HISTORY_TAG_BITS      = 8;
    static constexpr int LONG_HISTORY_TAG_BITS       = 12;
    static constexpr int PRED_COUNTER_WIDTH          = 3;
    static constexpr int USEFUL_BITS                 = 1;
    static constexpr int LOG_ENTRIES_PER_BANK        = 10;
    static constexpr int SHORT_HISTORY_NUM_BANKS     = 10;
    static constexpr int LONG_HISTORY_NUM_BANKS      = 20;
    static constexpr int EXTRA_ENTRIES_TO_ALLOCATE   = 1;
    static constexpr int TICKS_UNTIL_USEFUL_SHIFT    = 1024;
    static constexpr int ALT_SELECTOR_LOG_TABLE_SIZE = 4;
    static constexpr int ALT_SELECTOR_ENTRY_WIDTH    = 5;
    static constexpr int BIMODAL_HYSTERESIS_SHIFT    = 2;
    static constexpr int BIMODAL_LOG_TABLES_SIZE     = 13;
  };

  struct LOOP {
    static constexpr int LOG_NUM_ENTRIES         = 5;
    static constexpr int ITERATION_COUNTER_WIDTH = 10;
    static constexpr int TAG_BITS                = 10;
    static constexpr int CONFIDENCE_THRESHOLD    = 15;
  };

  struct SC {
    static constexpr int UPDATE_THRESHOLD_WIDTH       = 12;
    static constexpr int PERPC_UPDATE_THRESHOLD_WIDTH = 8;
    static constexpr int INITIAL_UPDATE_THRESHOLD     = 35 << 3;

    static constexpr bool USE_VARIABLE_THRESHOLD = true;
    static constexpr int  LOG_SIZE_PERPC_THRESHOLD_TABLE =
      USE_VARIABLE_THRESHOLD ? 6 : 0;
    static constexpr int LOG_SIZE_VARIABLE_THRESHOLD_TABLE =
      LOG_SIZE_PERPC_THRESHOLD_TABLE / 2;
    static constexpr int VARIABLE_THRESHOLD_WIDTH            = 6;
    static constexpr int INITIAL_VARIABLE_THRESHOLD          = 7;
    static constexpr int INITIAL_VARIABLE_THRESHOLD_FOR_BIAS = 4;
    static constexpr int LOG_BIAS_ENTRIES                    = 8;

    static constexpr int LOG_SIZE_GLOBAL_HISTORY_GEHL = 10;
    struct GLOBAL_HISTORY_GEHL_HISTORIES {
      static constexpr int arr[] = {40, 24, 10};
    };
    static constexpr int LOG_SIZE_PATH_GEHL = 9;
    struct PATH_GEHL_HISTORIES {
      static constexpr int arr[] = {25, 16, 9};
    };

    static constexpr bool USE_LOCAL_HISTORY                  = true;
    static constexpr int  FIRST_LOCAL_HISTORY_LOG_TABLE_SIZE = 8;
    static constexpr int  FIRST_LOCAL_HISTORY_SHIFT          = 2;
    static constexpr int  LOG_SIZE_FIRST_LOCAL_GEHL          = 10;
    struct FIRST_LOCAL_GEHL_HISTORIES {
      static constexpr int arr[] = {11, 6, 3};
    };

    static constexpr bool USE_SECOND_LOCAL_HISTORY            = true;
    static constexpr int  SECOND_LOCAL_HISTORY_LOG_TABLE_SIZE = 4;
    static constexpr int  SECOND_LOCAL_HISTORY_SHIFT          = 5;
    static constexpr int  LOG_SIZE_SECOND_LOCAL_GEHL          = 9;
    struct SECOND_LOCAL_GEHL_HISTORIES {
      static constexpr int arr[] = {16, 11, 6};
    };

    static constexpr bool USE_THIRD_LOCAL_HISTORY            = true;
    static constexpr int  THIRD_LOCAL_HISTORY_LOG_TABLE_SIZE = 4;
    static constexpr int  THIRD_LOCAL_HISTORY_SHIFT          = 10;
    static constexpr int  LOG_SIZE_THIRD_LOCAL_GEHL          = 10;
    struct THIRD_LOCAL_GEHL_HISTORIES {
      static constexpr int arr[] = {9, 4};
    };

    static constexpr bool USE_IMLI                 = true;
    static constexpr int  IMLI_COUNTER_WIDTH       = 8;
    static constexpr int  IMLI_TABLE_SIZE          = 1 << IMLI_COUNTER_WIDTH;
    static constexpr int  log_size_first_imli_gehl = 8;
    struct FIRST_IMLI_GEHL_HISTORIES {
      static constexpr int arr[] = {IMLI_COUNTER_WIDTH};
    };
    static constexpr int LOG_SIZE_SECOND_IMLI_GEHL = 9;
    struct SECOND_IMLI_GEHL_HISTORIES {
      static constexpr int arr[] = {10, 4};
    };

    static constexpr int PRECISION             = 6;
    static constexpr int SC_PATH_HISTORY_WIDTH = 27;
  };
};

constexpr int TAGE_SC_L_CONFIG_64KB::SC::GLOBAL_HISTORY_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_64KB::SC::PATH_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_64KB::SC::FIRST_LOCAL_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_64KB::SC::SECOND_LOCAL_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_64KB::SC::THIRD_LOCAL_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_64KB::SC::FIRST_IMLI_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_64KB::SC::SECOND_IMLI_GEHL_HISTORIES::arr[];

/****************************************************************************************/
struct TAGE_SC_L_CONFIG_80KB {
  // static constexpr bool PIPELINE_SUPPORT = true;
  static constexpr bool USE_LOOP_PREDICTOR       = true;
  static constexpr bool USE_SC                   = true;
  static constexpr int  CONFIDENCE_COUNTER_WIDTH = 7;

  struct TAGE {
    static constexpr int MIN_HISTORY_SIZE            = 6;
    static constexpr int MAX_HISTORY_SIZE            = 3000;
    static constexpr int NUM_HISTORIES               = 18;
    static constexpr int PATH_HISTORY_WIDTH          = 27;
    static constexpr int FIRST_LONG_HISTORY_TABLE    = 13;
    static constexpr int FIRST_2WAY_TABLE            = 9;
    static constexpr int LAST_2WAY_TABLE             = 22;
    static constexpr int SHORT_HISTORY_TAG_BITS      = 8;
    static constexpr int LONG_HISTORY_TAG_BITS       = 12;
    static constexpr int PRED_COUNTER_WIDTH          = 3;
    static constexpr int USEFUL_BITS                 = 1;
    static constexpr int LOG_ENTRIES_PER_BANK        = 10;
    static constexpr int SHORT_HISTORY_NUM_BANKS     = 18;
    static constexpr int LONG_HISTORY_NUM_BANKS      = 21;
    static constexpr int EXTRA_ENTRIES_TO_ALLOCATE   = 1;
    static constexpr int TICKS_UNTIL_USEFUL_SHIFT    = 1024;
    static constexpr int ALT_SELECTOR_LOG_TABLE_SIZE = 4;
    static constexpr int ALT_SELECTOR_ENTRY_WIDTH    = 5;
    static constexpr int BIMODAL_HYSTERESIS_SHIFT    = 2;
    static constexpr int BIMODAL_LOG_TABLES_SIZE     = 13;
  };

  struct LOOP {
    static constexpr int LOG_NUM_ENTRIES         = 5;
    static constexpr int ITERATION_COUNTER_WIDTH = 10;
    static constexpr int TAG_BITS                = 10;
    static constexpr int CONFIDENCE_THRESHOLD    = 15;
  };

  struct SC {
    static constexpr int UPDATE_THRESHOLD_WIDTH       = 12;
    static constexpr int PERPC_UPDATE_THRESHOLD_WIDTH = 8;
    static constexpr int INITIAL_UPDATE_THRESHOLD     = 35 << 3;

    static constexpr bool USE_VARIABLE_THRESHOLD = true;
    static constexpr int  LOG_SIZE_PERPC_THRESHOLD_TABLE =
      USE_VARIABLE_THRESHOLD ? 6 : 0;
    static constexpr int LOG_SIZE_VARIABLE_THRESHOLD_TABLE =
      LOG_SIZE_PERPC_THRESHOLD_TABLE / 2;
    static constexpr int VARIABLE_THRESHOLD_WIDTH            = 6;
    static constexpr int INITIAL_VARIABLE_THRESHOLD          = 7;
    static constexpr int INITIAL_VARIABLE_THRESHOLD_FOR_BIAS = 4;
    static constexpr int LOG_BIAS_ENTRIES                    = 8;

    static constexpr int LOG_SIZE_GLOBAL_HISTORY_GEHL = 10;
    struct GLOBAL_HISTORY_GEHL_HISTORIES {
      static constexpr int arr[] = {40, 24, 10};
    };
    static constexpr int LOG_SIZE_PATH_GEHL = 9;
    struct PATH_GEHL_HISTORIES {
      static constexpr int arr[] = {25, 16, 9};
    };

    static constexpr bool USE_LOCAL_HISTORY                  = true;
    static constexpr int  FIRST_LOCAL_HISTORY_LOG_TABLE_SIZE = 8;
    static constexpr int  FIRST_LOCAL_HISTORY_SHIFT          = 2;
    static constexpr int  LOG_SIZE_FIRST_LOCAL_GEHL          = 10;
    struct FIRST_LOCAL_GEHL_HISTORIES {
      static constexpr int arr[] = {11, 6, 3};
    };

    static constexpr bool USE_SECOND_LOCAL_HISTORY            = true;
    static constexpr int  SECOND_LOCAL_HISTORY_LOG_TABLE_SIZE = 4;
    static constexpr int  SECOND_LOCAL_HISTORY_SHIFT          = 5;
    static constexpr int  LOG_SIZE_SECOND_LOCAL_GEHL          = 9;
    struct SECOND_LOCAL_GEHL_HISTORIES {
      static constexpr int arr[] = {16, 11, 6};
    };

    static constexpr bool USE_THIRD_LOCAL_HISTORY            = true;
    static constexpr int  THIRD_LOCAL_HISTORY_LOG_TABLE_SIZE = 4;
    static constexpr int  THIRD_LOCAL_HISTORY_SHIFT          = 10;
    static constexpr int  LOG_SIZE_THIRD_LOCAL_GEHL          = 10;
    struct THIRD_LOCAL_GEHL_HISTORIES {
      static constexpr int arr[] = {9, 4};
    };

    static constexpr bool USE_IMLI                 = true;
    static constexpr int  IMLI_COUNTER_WIDTH       = 8;
    static constexpr int  IMLI_TABLE_SIZE          = 1 << IMLI_COUNTER_WIDTH;
    static constexpr int  log_size_first_imli_gehl = 8;
    struct FIRST_IMLI_GEHL_HISTORIES {
      static constexpr int arr[] = {IMLI_COUNTER_WIDTH};
    };
    static constexpr int LOG_SIZE_SECOND_IMLI_GEHL = 9;
    struct SECOND_IMLI_GEHL_HISTORIES {
      static constexpr int arr[] = {10, 4};
    };

    static constexpr int PRECISION             = 8;
    static constexpr int SC_PATH_HISTORY_WIDTH = 27;
  };
};

constexpr int TAGE_SC_L_CONFIG_80KB::SC::GLOBAL_HISTORY_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_80KB::SC::PATH_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_80KB::SC::FIRST_LOCAL_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_80KB::SC::SECOND_LOCAL_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_80KB::SC::THIRD_LOCAL_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_80KB::SC::FIRST_IMLI_GEHL_HISTORIES::arr[];
constexpr int TAGE_SC_L_CONFIG_80KB::SC::SECOND_IMLI_GEHL_HISTORIES::arr[];