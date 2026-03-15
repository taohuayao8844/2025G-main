/**
 * @file    Mfft.c
 * @brief   FFT 分析与扫频测量模块实现
 *          动态采样率 + ADC1/ADC2 双重同步 + 增益/相位计算
 */
#include "Mfft.h"
#include "adc.h"
#include "tim.h"
#include <stdio.h>
#include <math.h>

/*---------------------------------------------------------------------------
 * DDS 接口（__weak，用户在其他文件中实现即可覆盖）
 *---------------------------------------------------------------------------*/
__weak void DDS_SetFreq(uint32_t freq_hz) { (void)freq_hz; }
__weak void DDS_SetMag(uint32_t mag)      { (void)mag; }

/*---------------------------------------------------------------------------
 * 模块内部变量
 *---------------------------------------------------------------------------*/

/* DMA 目标缓冲区（32位打包：低16位=ADC1，高16位=ADC2） */
uint32_t mfft_dual_buffer[MFFT_SAMPLE_COUNT];

/* 分离后的 ADC 数据 */
static uint16_t adc1_data[MFFT_SAMPLE_COUNT];
static uint16_t adc2_data[MFFT_SAMPLE_COUNT];

/* DMA 完成标志（由 main.c 中的 ADC 回调设置） */
volatile uint8_t mfft_buffer_ready = 0;

/* FFT 复数输入/幅度输出 */
static float fft_input[MFFT_SAMPLE_COUNT * 2];
static float fft_output[MFFT_SAMPLE_COUNT];

/* FFT 实例 */
static arm_cfft_radix4_instance_f32 fft_instance;

/* 当前实际采样率（用于串口输出） */
static uint32_t current_sample_rate = 0;

/*---------------------------------------------------------------------------
 * MFFT_Init - 初始化 FFT 实例
 *---------------------------------------------------------------------------*/
void MFFT_Init(void)
{
  arm_cfft_radix4_init_f32(&fft_instance, MFFT_SAMPLE_COUNT, 0, 1);
}

/*---------------------------------------------------------------------------
 * MFFT_GetOptimalSampleRate - 根据信号频率计算最优采样率
 * 策略：采样率 = 信号频率 × MFFT_OVERSAMPLE_RATIO
 *       限制在 [MFFT_MIN_SAMPLE_RATE, MFFT_MAX_SAMPLE_RATE] 范围内
 *---------------------------------------------------------------------------*/
uint32_t MFFT_GetOptimalSampleRate(uint32_t signal_freq)
{
  uint32_t target = signal_freq * MFFT_OVERSAMPLE_RATIO;

  if (target < MFFT_MIN_SAMPLE_RATE)
    target = MFFT_MIN_SAMPLE_RATE;
  if (target > MFFT_MAX_SAMPLE_RATE)
    target = MFFT_MAX_SAMPLE_RATE;

  return target;
}

/*---------------------------------------------------------------------------
 * MFFT_SetSampleRate - 动态修改 TIM3 ARR 以改变采样率
 * 返回实际采样率（因为整数除法会有误差）
 *---------------------------------------------------------------------------*/
uint32_t MFFT_SetSampleRate(uint32_t sample_rate)
{
  /* 计算 TIM3 ARR 值 */
  uint32_t arr = MFFT_TIM_CLK / sample_rate;
  if (arr == 0) arr = 1;

  /* 直接修改 ARR 寄存器，下一个更新事件生效 */
  __HAL_TIM_SET_AUTORELOAD(&htim3, arr - 1);

  /* 计算实际采样率 */
  current_sample_rate = MFFT_TIM_CLK / arr;

  return current_sample_rate;
}

/*---------------------------------------------------------------------------
 * MFFT_CalcResponse - FFT 计算增益和相位差
 * 在峰值频率 bin 处提取幅度和相位
 *---------------------------------------------------------------------------*/
void MFFT_CalcResponse(uint16_t *adc1_buf, uint16_t *adc2_buf,
                       float *out_gain_db, float *out_phase_deg)
{
  uint32_t i;
  float adc1_mag = 0.0f;
  float adc2_mag = 0.0f;
  uint32_t peak_index = 0;
  float adc1_real, adc1_imag;
  float adc2_real, adc2_imag;

  /* ---- ADC1（输入信号）FFT ---- */
  for (i = 0; i < MFFT_SAMPLE_COUNT; i++)
  {
    fft_input[2 * i]     = (float)adc1_buf[i] / 4095.0f;
    fft_input[2 * i + 1] = 0.0f;
  }

  arm_cfft_radix4_f32(&fft_instance, fft_input);
  arm_cmplx_mag_f32(fft_input, fft_output, MFFT_SAMPLE_COUNT);

  /* 跳过 DC，找峰值 bin */
  arm_max_f32(fft_output + 1, MFFT_SAMPLE_COUNT / 2 - 1,
              &adc1_mag, &peak_index);
  peak_index++;

  /* 保存峰值 bin 处的复数值用于相位计算 */
  adc1_real = fft_input[2 * peak_index];
  adc1_imag = fft_input[2 * peak_index + 1];

  /* ---- ADC2（输出信号）FFT ---- */
  for (i = 0; i < MFFT_SAMPLE_COUNT; i++)
  {
    fft_input[2 * i]     = (float)adc2_buf[i] / 4095.0f;
    fft_input[2 * i + 1] = 0.0f;
  }

  arm_cfft_radix4_f32(&fft_instance, fft_input);
  arm_cmplx_mag_f32(fft_input, fft_output, MFFT_SAMPLE_COUNT);

  /* 在同一 bin 处取幅度和复数值 */
  adc2_mag  = fft_output[peak_index];
  adc2_real = fft_input[2 * peak_index];
  adc2_imag = fft_input[2 * peak_index + 1];

  /* ---- 增益计算 ---- */
  if (adc1_mag > 0.001f)
  {
    *out_gain_db = 20.0f * log10f(adc2_mag / adc1_mag);
  }
  else
  {
    *out_gain_db = -999.0f;
  }

  /* ---- 相位差计算，归一化到 [-180, 180] ---- */
  float phase1 = atan2f(adc1_imag, adc1_real);
  float phase2 = atan2f(adc2_imag, adc2_real);
  *out_phase_deg = (phase2 - phase1) * (180.0f / 3.14159265f);

  if (*out_phase_deg > 180.0f)
    *out_phase_deg -= 360.0f;
  else if (*out_phase_deg < -180.0f)
    *out_phase_deg += 360.0f;
}

/*---------------------------------------------------------------------------
 * MFFT_PerformSweep - 执行一次完整扫频
 * 对数分布频率点，每个点动态调整采样率
 * 串口输出 CSV 格式：Freq(Hz),Fs(Hz),Gain(dB),Phase(deg)
 *---------------------------------------------------------------------------*/
void MFFT_PerformSweep(void)
{
  uint32_t i;

  printf("SWEEP_START\r\n");
  printf("Freq(Hz),Fs(Hz),Gain(dB),Phase(deg)\r\n");

  for (i = 0; i < MFFT_SWEEP_POINTS; i++)
  {
    /* 对数分布计算当前频率 */
    float ratio = (float)i / (float)(MFFT_SWEEP_POINTS - 1);
    float freq  = (float)MFFT_SWEEP_START_FREQ
                  * powf((float)MFFT_SWEEP_END_FREQ / (float)MFFT_SWEEP_START_FREQ,
                         ratio);
    uint32_t freq_hz = (uint32_t)(freq + 0.5f);

    /* 动态调整采样率 */
    uint32_t target_fs = MFFT_GetOptimalSampleRate(freq_hz);
    uint32_t actual_fs = MFFT_SetSampleRate(target_fs);

    /* 设置 DDS 输出频率 */
    DDS_SetFreq(freq_hz);

    /* 等待信号和采样率稳定 */
    HAL_Delay(MFFT_SETTLE_MS);

    /* 丢弃旧数据，等待一轮新的完整采样 */
    mfft_buffer_ready = 0;
    while (!mfft_buffer_ready);
    mfft_buffer_ready = 0;

    /* 从双重缓冲区提取 ADC1 和 ADC2 数据 */
    for (uint32_t j = 0; j < MFFT_SAMPLE_COUNT; j++)
    {
      adc1_data[j] = (uint16_t)(mfft_dual_buffer[j] & 0xFFFF);
      adc2_data[j] = (uint16_t)(mfft_dual_buffer[j] >> 16);
    }

    /* FFT 计算增益和相位差 */
    float gain, phase;
    MFFT_CalcResponse(adc1_data, adc2_data, &gain, &phase);

    /* CSV 输出：频率, 采样率, 增益, 相位 */
    printf("%lu,%lu,%.2f,%.2f\r\n", freq_hz, actual_fs, gain, phase);
  }

  printf("SWEEP_END\r\n\r\n");
}
