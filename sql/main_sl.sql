
CREATE TABLE "device"
(
  "id" INTEGER PRIMARY KEY,
  "addr_i2c" INTEGER NOT NULL-- to get chip address in hex format (for Banana Pi): i2cdetect -y -a 2; convert it to decimal and put here 
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
  "pwm_period_nsec" INTEGER NOT NULL
);
