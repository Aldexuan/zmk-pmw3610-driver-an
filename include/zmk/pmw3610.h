/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Change the PMW3610 runtime CPI for the MOVE input mode.
 *
 * The value is clamped to the allowed PMW3610 CPI range (200 .. 3200) and
 * rounded down to the nearest 200-cpi step, matching the sensor's resolution
 * register granularity.
 *
 * @param amount Delta (in CPI units, e.g. +200 or -200) to apply to the
 *               current runtime CPI. Positive increases CPI, negative
 *               decreases CPI.
 *
 * @retval 0 on success.
 * @retval -ENODEV if no PMW3610 device is available.
 * @retval Negative errno on SPI / sensor errors.
 */
int zmk_pmw3610_cpi_change(int amount);

/**
 * @brief Get the current runtime MOVE-mode CPI.
 *
 * @return Current runtime CPI, or -ENODEV if no device is available.
 */
int zmk_pmw3610_cpi_get(void);

#ifdef __cplusplus
}
#endif
