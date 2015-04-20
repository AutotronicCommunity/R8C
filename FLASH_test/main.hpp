#pragma once
//=====================================================================//
/*!	@file
	@brief	R8C メイン・ヘッダー
	@author	平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include "common/uart_io.hpp"
#include "common/trb_io.hpp"
#include "common/adc_io.hpp"
#include "common/flash_io.hpp"

typedef device::uart_io<device::UART0, 16, 16> uart0;

typedef device::trb_io timer_b;

typedef device::adc_io adc;

typedef device::flash_io flash;