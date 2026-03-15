/**
 * @file    Mfft.h
 * @brief   FFT 分析与扫频测量模块
 *          基于 ADC1/ADC2 双重同步采样，动态调整采样率
 */
#ifndef __MFFT_H
#define __MFFT_H

#include "main.h"
#include "arm_math.h"

/*---------------------------------------------------------------------------
 * 配置参数（可根据需要修改）
 *---------------------------------------------------------------------------*/
#define MFFT_SAMPLE_COUNT       1024      /* FFT 点数 */
#define MFFT_SWEEP_START_FREQ   1000      /* 扫频起始频率 Hz */
#define MFFT_SWEEP_END_FREQ     100000    /* 扫频结束频率 Hz */
#define MFFT_SWEEP_POINTS       50        /* 扫频点数（对数分布） */
#define MFFT_SETTLE_MS          50        /* 频率切换后等待稳定 ms */
#define MFFT_OVERSAMPLE_RATIO   10        /* 采样率 = 信号频率 × 此倍数 */
#define MFFT_MIN_SAMPLE_RATE    10000     /* 最低采样率 10kHz */
#define MFFT_MAX_SAMPLE_RATE    1000000   /* 最高采样率 1MHz */
#define MFFT_TIM_CLK            42000000  /* TIM3 计数频率：84MHz / PSC(2) */

/*---------------------------------------------------------------------------
 * 扫频结果结构体
 *---------------------------------------------------------------------------*/
typedef struct
{
  uint32_t frequency;   /* 信号频率 Hz */
  uint32_t sample_rate; /* 实际采样率 Hz */
  float    gain_db;     /* 增益 dB */
  float    phase_deg;   /* 相位差 deg */
} MFFT_SweepPoint;

/*---------------------------------------------------------------------------
 * 外部可见的 DMA 完成标志（由 ADC 回调设置）
 *---------------------------------------------------------------------------*/
extern volatile uint8_t mfft_buffer_ready;

/*---------------------------------------------------------------------------
 * 外部可见的数据缓冲区（DMA 目标）
 *---------------------------------------------------------------------------*/
extern uint32_t mfft_dual_buffer[MFFT_SAMPLE_COUNT];

/*---------------------------------------------------------------------------
 * 公共接口
 *---------------------------------------------------------------------------*/

/**
 * @brief  初始化 FFT 模块
 */
void MFFT_Init(void);

/**
 * @brief  根据信号频率计算最优采样率
 * @param  signal_freq: 信号频率 Hz
 * @return 实际采样率 Hz
 */
uint32_t MFFT_GetOptimalSampleRate(uint32_t signal_freq);

/**
 * @brief  动态设置采样率（修改 TIM3 ARR）
 * @param  sample_rate: 目标采样率 Hz
 * @return 实际采样率 Hz
 */
uint32_t MFFT_SetSampleRate(uint32_t sample_rate);

/**
 * @brief  对两路 ADC 数据做 FFT，计算增益和相位差
 * @param  adc1_buf: ADC1 输入信号数据
 * @param  adc2_buf: ADC2 输出信号数据
 * @param  out_gain_db: 输出增益 dB
 * @param  out_phase_deg: 输出相位差 deg
 */
void MFFT_CalcResponse(uint16_t *adc1_buf, uint16_t *adc2_buf,
                       float *out_gain_db, float *out_phase_deg);

/**
 * @brief  执行一次完整扫频，串口输出 CSV 数据
 */
void MFFT_PerformSweep(void);

#endif /* __MFFT_H */
