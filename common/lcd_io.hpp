#pragma once
//=====================================================================//
/*!	@file
	@brief	ST7567 LCD ドライバー
	@author	平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include <cstdint>
#include "common/spi_io.hpp"

namespace device {

	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	/*!
		@brief  LCD テンプレートクラス @n
				PORT ポート指定クラス： @n
				class port { @n
				public: @n
					void scl_out(bool val) const { } @n
					void sda_out(bool val) const { } @n
				};
		@param[in]	PORT	SPI ポート定義クラス
		@param[in]	CTRL	SPI コントロール定義クラス
	*/
	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
	template <class PORT, class CTRL>
	class lcd_io {

		spi_io<PORT>	spi_;
		CTRL			ctrl_;

	public:
		//-----------------------------------------------------------------//
		/*!
			@brief  開始
		*/
		//-----------------------------------------------------------------//
		void start() const {
			spi_.init();
			ctrl_.init();

			// 100ms setup...
			for(uint8_t i = 0; i < 10; ++i) {
				utils::delay::micro_second(10000);
			}

			ctrl_.a0_out(0);
			ctrl_.cs_out(0);	// device enable

			spi_.write(0xae);	// display off
			spi_.write(0x40);	// display start line of 0
			spi_.write(0xa1);	// ADC set to reverse
			spi_.write(0xc0);	// common output mode: set scan direction normal operation
			spi_.write(0xa6);	// display normal (none reverse)
			spi_.write(0xa2);	// select bias b0:0=1/9, b0:1=1/7
			spi_.write(0x2f);	// all power control circuits on

			spi_.write(0xf8);	// set booster ratio to
			spi_.write(0x00);	// 4x

			spi_.write(0x27);	// set V0 voltage resistor ratio to large

			spi_.write(0x81);	// set contrast
			spi_.write(0x0);	// contrast value, EA default: 0x016

			spi_.write(0xac);	// indicator
			spi_.write(0x00);	// disable

			spi_.write(0xa4);	// all pixel on disable
			spi_.write(0xaf);	// display on

			for(uint8_t i = 0; i < 5; ++i) {
				utils::delay::micro_second(10000);
			}

			ctrl_.cs_out(1);	// device disable
		}


		//-----------------------------------------------------------------//
		/*!
			@brief  コピー
			@param[in]	p	フレームバッファソース
		*/
		//-----------------------------------------------------------------//
		void copy(const uint8_t* p) const {
			ctrl_.cs_out(0);
			for(uint8_t page = 0; page < 4; ++page) {
				ctrl_.a0_out(0);
				spi_.write(0xb0 + page);
				spi_.write(0x10);  // column upper
				spi_.write(0x04);  // column lower
				ctrl_.a0_out(1);
				for(uint8_t i = 0; i < 128; ++i) {
					spi_.write(*p);
					++p;
				}
			}
			ctrl_.cs_out(1);
		}

	};
}
