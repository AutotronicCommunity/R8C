#pragma once
//=====================================================================//
/*!	@file
	@brief	EEPROM ドライバー
	@author	平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include <cstdint>
#include "common/i2c_io.hpp"
#include "common/time.h"

namespace device {

	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	/*!
		@brief  EEPROM テンプレートクラス @n
				PORT ポート指定クラス： @n
				class port { @n
				public: @n
					void scl_dir(bool val) const { } @n
					void scl_out(bool val) const { } @n
					bool scl_inp() const { return 0; } @n
					void sda_dir(bool val) const { } @n
					void sda_out(bool val) const { } @n
					bool sda_inp() const { return 0; } @n
				};
		@param[in]	PORT	ポート定義クラス
	*/
	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	template <class PORT>
	class eeprom_io {

		static const uint8_t	EEPROM_ADR_ = 0x50;

		i2c_io<PORT>&	i2c_io_;

		uint8_t	ds_;
		bool	exp_;
		uint8_t	pagen_;

		uint8_t i2c_adr_(bool exta = 0) const {
			return EEPROM_ADR_ | ds_ | (static_cast<uint8_t>(exta) << 2);
		}

	public:
		//-----------------------------------------------------------------//
		/*!
			@brief	コンストラクター
			@param[in]	i2c	i2c_io クラスを参照で渡す
		 */
		//-----------------------------------------------------------------//
		eeprom_io(i2c_io<PORT>& i2c) : i2c_io_(i2c), ds_(0), exp_(false), pagen_(1) { }


		//-----------------------------------------------------------------//
		/*!
			@brief	開始
			@param[in]	ds	デバイス選択ビット
			@param[in]	exp	「true」の場合、２バイトアドレス
			@param[in]	pagen	ページサイズ
		 */
		//-----------------------------------------------------------------//
		void start(uint8_t ds, bool exp, uint8_t pagen) {
			ds_ = ds & 7;
			exp_ = exp;
			pagen_ = pagen;
		}


		//-----------------------------------------------------------------//
		/*!
			@brief	書き込み同期
			@param[in]	adr	読み込みテストアドレス
			@param[in]	delay 待ち時間（10us単位）
			@return デバイスエラーなら「false」
		 */
		//-----------------------------------------------------------------//
		bool sync_write(uint32_t adr = 0, uint16_t delay = 600) const {
			bool ok = false;
			for(uint16_t i = 0; i < delay; ++i) {
				utils::delay::micro_second(10);
				uint8_t tmp[1];
				if(read(adr, tmp, 1)) {
					ok = true;
					break;
				}
			}
			return ok;
		}


		//-----------------------------------------------------------------//
		/*!
			@brief	EEPROM 読み出し
			@param[in]	adr	読み出しアドレス
			@param[out]	dst	先
			@param[in]	len	長さ
			@return 成功なら「true」
		 */
		//-----------------------------------------------------------------//
		bool read(uint32_t adr, uint8_t* dst, uint16_t len) const {
			if(exp_) {
				uint8_t tmp[2];
				tmp[0] = (adr >> 8) & 255;
				tmp[1] =  adr & 255;
				if(!i2c_io_.send(i2c_adr_((adr >> 16) & 1), tmp, 2)) {
					return false;
				}
				if(!i2c_io_.recv(i2c_adr_((adr >> 16) & 1), dst, len)) {
					return false;
				}
			} else {
				uint8_t tmp[1];
				tmp[0] = adr & 255;
				if(!i2c_io_.send(i2c_adr_(), tmp, 1)) {
					return false;
				}
				if(!i2c_io_.recv(i2c_adr_(), dst, len)) {
					return false;
				}
			}
			return true;
		}


		//-----------------------------------------------------------------//
		/*!
			@brief	EEPROM 書き込み
			@param[in]	adr	書き込みアドレス
			@param[out]	src	元
			@param[in]	len	長さ
			@return 成功なら「true」
		 */
		//-----------------------------------------------------------------//
		bool write(uint32_t adr, const uint8_t* src, uint16_t len) const {
			const uint8_t* end = src + len;
			while(src < end) {
				uint16_t l = pagen_ - (reinterpret_cast<uint16_t>(src) & (pagen_ - 1));
				if(exp_) {
					if(!i2c_io_.send(i2c_adr_((adr >> 16) & 1), adr >> 8, adr & 255, src, l)) {
						return false;
					}
				} else {
					if(!i2c_io_.send(i2c_adr_((adr >> 16) & 1), adr & 255, src, l)) {
						return false;
					}
				}
				src += l;
				adr += l;
				if(src < end) {  // 書き込み終了を待つポーリング
					if(!sync_write(adr)) {
						return false;
					}
				}
			}
			return true;
		}
	};
}
