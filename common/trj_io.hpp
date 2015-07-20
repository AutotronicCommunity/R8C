#pragma once
//=====================================================================//
/*!	@file
	@brief	R8C グループ・TimerRJ I/O 制御 @n
			Copyright 2015 Kunihito Hiramatsu
	@author	平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include "common/vect.h"
#include "system.hpp"
#include "intr.hpp"
#include "timer_rj.hpp"

/// F_CLK はタイマー周期計算で必要で、設定が無いとエラーにします。
#ifndef F_CLK
#  error "trj_io.hpp requires F_CLK to be defined"
#endif

namespace device {

	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	/*!
		@brief  TimerRJ I/O 制御クラス
		@param[in]	TASK 割り込み内で実行されるクラス
	*/
	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	template <class TASK>
	class trj_io {

		static TASK task_;

	public:
		//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
		/*!
			@brief  パルス計測モード
		*/
		//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
		enum class measurement {
			low_width,	///< Low レベル幅測定
			high_width,	///< High レベル幅測定
			freq,		///< 周波数測定
		};


		//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
		/*!
			@brief  フィルタータイプ
		*/
		//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
		enum class filter {
			none,		///< 無し
			f1 = 1,		///< f1 フィルター
			f8 = 2,		///< f8 フィルター
			f32 = 3,	///< f32 フィルター
		};


		static INTERRUPT_FUNC void itask() {
			task_();
		}

	private:
		bool set_freq_(uint32_t freq) const {
			uint32_t tn = F_CLK / (freq * 2);
			uint8_t cks = 0;
			while(tn > 65536) {
				tn >>= 1;
				++cks;
				if(cks == 2) {
					tn >>= 1;
				}
				if(cks >= 3) return false;
			}
			if(tn) --tn;
			else return false;

			static const uint8_t tbl_[3] = { 0, 3, 1 }; // 1/1, 1/2, 1/8

			TRJMR = TRJMR.TCK.b(tbl_[cks]) | TRJMR.TCKCUT.b(0) | TRJMR.TMOD.b(1);  // パルス出力モード
			TRJ = tn;

			return true;
		}

	public:
		//-----------------------------------------------------------------//
		/*!
			@brief  コンストラクター
		*/
		//-----------------------------------------------------------------//
		trj_io() { }


		//-----------------------------------------------------------------//
		/*!
			@brief  パルス出力の開始（TRJIO/TRJO 端子から、パルスを出力）
			@param[in]	freq	周波数
			@param[in]	ir_lvl	割り込みレベル（０の場合割り込みを使用しない）
			@return 設定範囲を超えたら「false」
		*/
		//-----------------------------------------------------------------//
		bool pluse_out(uint32_t freq, uint8_t ir_lvl = 0) const {
			MSTCR.MSTTRJ = 0;  // モジュールスタンバイ解除

			TRJCR.TSTART = 0;  // カウンタを停止

			if(!set_freq_(freq)) {
				return false;
			}

			TRJIOC.TEDGSEL = 1;  // L から出力
			TRJIOC.TOPCR = 0;    // トグル出力

			ILVLB.B01 = ir_lvl;
			if(ir_lvl) {
				TRJIR.TRJIE = 1;
			} else {
				TRJIR.TRJIE = 0;
			}

			TRJCR.TSTART = 1;  // カウンタを開始

			return true;
		}


		//-----------------------------------------------------------------//
		/*!
			@brief  TRJ 出力周波数の設定
			@param[in]	freq	周波数
			@return 設定範囲を超えたら「false」
		*/
		//-----------------------------------------------------------------//
		bool set_cycle(uint32_t freq) const {
			return set_freq_(freq);
		}


		//-----------------------------------------------------------------//
		/*!
			@brief  パルス計測の開始（TRJIO 端子から、パルスを入力）
			@param[in]	measur	パルス計測のモード
			@param[in]	fil		入力フィルター
			@param[in]	ir_lvl	割り込みレベル（０の場合割り込みを使用しない）
		*/
		//-----------------------------------------------------------------//
		void pluse_inp(measurement measur, filter fil = filter::none, uint8_t ir_lvl = 0) const {
			MSTCR.MSTTRJ = 0;  // モジュールスタンバイ解除

			TRJCR.TSTART = 0;  // カウンタを停止

			TRJIOC = TRJIOC.TIPF.b(static_cast<uint8_t>(fil)) | TRJIOC.TOPCR.b(1);
//			TRJMR  = 

			TRJ = 0;

			TRJCR.TSTART = 1;  // カウンタを開始
		}


		//-----------------------------------------------------------------//
		/*!
			@brief  TRJ カウント値を取得
			@return カウント値
		*/
		//-----------------------------------------------------------------//
		uint16_t get_count() const {
			return TRJ();
		}

	};
}
