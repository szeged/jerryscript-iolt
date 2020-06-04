#include "esp_arducam_js.h"

extern const struct sensor_reg ov5642_RAW[];
extern const struct sensor_reg OV5642_1280x960_RAW[];
extern const struct sensor_reg OV5642_1920x1080_RAW[];
extern const struct sensor_reg OV5642_640x480_RAW[];
extern const struct sensor_reg ov5642_320x240[];
extern const struct sensor_reg ov5642_640x480[];
extern const struct sensor_reg ov5642_1280x960[];
extern const struct sensor_reg ov5642_1600x1200[];
extern const struct sensor_reg ov5642_1024x768[];
extern const struct sensor_reg ov5642_2048x1536[];
extern const struct sensor_reg ov5642_2592x1944[];
extern const struct sensor_reg ov5642_dvp_zoom8[];
extern const struct sensor_reg OV5642_QVGA_Preview[];
extern const struct sensor_reg OV5642_JPEG_Capture_QSXGA[];
extern const struct sensor_reg OV5642_1080P_Video_setting[];
extern const struct sensor_reg OV5642_720P_Video_setting[];

#define pgm_read_byte(x)        (*((char *)x))
#define pgm_read_word(x)        ( ((*((unsigned char *)x + 1)) << 8) + (*((unsigned char *)x)))

bool wait (uint32_t timeout)
{
  vTaskDelay(timeout / portTICK_PERIOD_MS);
  return true;
}

uint8_t read_reg (uint8_t pin, uint8_t address)
{
  spi_cs_low (pin);
  // address is masked with 0b01111111 to make the first bit 0.
  spi_transfer_8 (SPI_BUS, address & 0x7f);
  uint8_t ret = spi_read_byte ();
  // correction for bit rotation from readback
  // ret = (uint8_t)(ret >> 1) | (ret << 7);
  spi_cs_high (pin);
  return ret;
}

void write_reg (uint8_t pin, uint8_t address, uint8_t value)
{
  spi_cs_low (pin);

  spi_transfer_8 (SPI_BUS, address | 0x80);
  spi_transfer_8 (SPI_BUS, value);

  spi_cs_high (pin);
}

uint8_t get_bit (uint8_t pin, uint8_t address, uint8_t bit)
{
  return read_reg (pin, address) & bit;
}

void set_bit (uint8_t pin, uint8_t address, uint8_t bit)
{
  write_reg (pin, address, read_reg (pin, address) | bit);
}

void clear_bit (uint8_t pin, uint8_t address, uint8_t bit)
{
  write_reg (pin, address, read_reg (pin, address) & (~bit));
}

uint32_t read_fifo_length ()
{
  uint8_t len1, len2, len3;
  uint32_t length = 0;
  // spi_cs_high (SD_CS);
  len1 = read_reg (CAMERA_CS, REG_FIFO_SIZE1);
  len2 = read_reg (CAMERA_CS, REG_FIFO_SIZE2);
  len3 = read_reg (CAMERA_CS, REG_FIFO_SIZE3) & 0x7f;
  length = ((len3 << 16) | (len2 << 8) | len1) & 0x07fffff;
  return length;
}

void write_buff_i2c (uint8_t *buff, uint8_t len, bool stop)
{
  i2c_start (I2C_BUS);
  for (uint8_t i = 0; i < len; i++)
  {
    i2c_write (I2C_BUS, buff[i]);
  }
  if (stop)
  {
    i2c_stop (I2C_BUS);
  }
}

void wr_sensor_reg_16_8 (uint16_t reg_id, uint8_t reg_dat)
{
  // Write 8 bit value to 16 bit register.
  uint8_t buff[4] = {
    I2C_SLAVE_ADDR_WRITE,
    reg_id >> 8,
    reg_id & 0x00ff,
    reg_dat & 0x00ff
  };
  write_buff_i2c (buff, 4, true);
  wait(10);
}

void wr_sensor_regs_16_8 (const struct sensor_reg reglist[])
{
  uint16_t reg_addr;
  uint8_t reg_val;

  const struct sensor_reg *next = reglist;

  printf ("wr_sensor_regs_16_8\n");

  while ((reg_addr != 0xffff) | (reg_val != 0xff))
  {
    reg_addr = pgm_read_word (&(next->reg_addr));
    reg_val = pgm_read_word (&(next->reg_val));
    wr_sensor_reg_16_8 (reg_addr, reg_val);
    next++;
  }
}

uint8_t rd_sensor_reg_16_8 (uint16_t regID)
{
  // Read 8 bit value from 16 bit register.
  uint8_t buff[4] = {
    I2C_SLAVE_ADDR_WRITE,
    regID >> 8,
    regID & 0x00ff,
    I2C_SLAVE_ADDR_READ
  };
  write_buff_i2c (buff, 3, true);
  write_buff_i2c (buff + 3, 1, false);
  uint8_t ret = i2c_read (I2C_BUS, true);
  i2c_stop (I2C_BUS);
  return ret;
}

void init_cam ()
{
  wr_sensor_reg_16_8 (0x3008, 0x80);
  wr_sensor_regs_16_8 (OV5642_QVGA_Preview);
  wait (200);
  wr_sensor_regs_16_8 (OV5642_JPEG_Capture_QSXGA);
  wr_sensor_regs_16_8 (ov5642_320x240);
  wait (100);
  wr_sensor_reg_16_8 (0x3818, 0xa8);
  wr_sensor_reg_16_8 (0x3621, 0x10);
  wr_sensor_reg_16_8 (0x3801, 0xb0);
  wr_sensor_reg_16_8 (0x4407, 0x04);
}

void set_image_size (enum image_size size)
{
  switch (size)
  {
    case OV5642_320x240:
      wr_sensor_regs_16_8 (ov5642_320x240);
      break;
    case OV5642_640x480:
      wr_sensor_regs_16_8 (ov5642_640x480);
      break;
    case OV5642_1024x768:
      wr_sensor_regs_16_8 (ov5642_1024x768);
      break;
    case OV5642_1280x960:
      wr_sensor_regs_16_8 (ov5642_1280x960);
      break;
    case OV5642_1600x1200:
      wr_sensor_regs_16_8 (ov5642_1600x1200);
      break;
    case OV5642_2048x1536:
      wr_sensor_regs_16_8 (ov5642_2048x1536);
      break;
    case OV5642_2592x1944:
      wr_sensor_regs_16_8 (ov5642_2592x1944);
      break;
    default:
      wr_sensor_regs_16_8 (ov5642_320x240);
      break;
  }
}
