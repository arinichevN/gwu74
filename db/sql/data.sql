CREATE TABLE "device"
(
  "id" INTEGER PRIMARY KEY,
  "i2c_path" TEXT NOT NULL,
  "i2c_addr" INTEGER NOT NULL-- to get chip address in hex format (for Banana Pi): i2cdetect -y -a 2; convert it to decimal and put here 
);
CREATE TABLE "pin"
(
  "net_id" INTEGER NOT NULL,
  "device_id" INTEGER NOT NULL,
  "id_within_device" INTEGER NOT NULL,
  "mode" TEXT NOT NULL, --in || out
  "pud" TEXT NOT NULL, -- up || down || off
  "rsl" INTEGER NOT NULL,--max duty cycle
  "pwm_period_sec" INTEGER NOT NULL,
  "pwm_period_nsec" INTEGER NOT NULL,
  "secure_timeout_sec" INTEGER NOT NULL, -- if secure_enable, we will set this pin to PWM mode with secure_duty_cycle after we have no requests to this pin while secure_timeout_sec is running
  "secure_duty_cycle" INTEGER NOT NULL, -- 0...rsl
  "secure_enable" INTEGER NOT NULL 
);

