
--DROP SCHEMA if exists gwu74 CASCADE;

CREATE SCHEMA gwu74;

CREATE TABLE gwu74.config
(
  app_class character varying(32) NOT NULL,
  db_public character varying (256) NOT NULL,
  udp_port character varying(32) NOT NULL,
  i2c_path character varying(32) NOT NULL,
  pid_path character varying(32) NOT NULL,
  udp_buf_size character varying(32) NOT NULL, --size of buffer used in sendto() and recvfrom() functions (508 is safe, if more then packet may be lost while transferring over network). Enlarge it if your rio module returns SRV_BUF_OVERFLOW
  db_data character varying(32) NOT NULL,--db connection string where gwu74 schema tables are located (except config table)
  cycle_duration_us character varying(32) NOT NULL,
  device_name character varying(32) NOT NULL,
 CONSTRAINT config_pkey PRIMARY KEY (app_class)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE gwu74.device
(
  app_class character varying(32) NOT NULL,
  id integer NOT NULL,
  addr_i2c integer NOT NULL,-- to get chip address in hex format (for Banana Pi): i2cdetect -y -a 2; convert it to decimal and put here 
  CONSTRAINT device_pkey PRIMARY KEY (app_class, id)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE gwu74.pin
(
  app_class character varying(32) NOT NULL,
  net_id integer NOT NULL,
  device_id integer NOT NULL,
  id_within_device integer NOT NULL,
  mode character varying (32) NOT NULL default 'out', --in || out
  pud character varying (32) NOT NULL default 'off', -- up || down || off
  pwm_period_sec integer NOT NULL default 20,
  pwm_period_nsec integer NOT NULL default 0,
  CONSTRAINT pin_pkey PRIMARY KEY (app_class, device_id, id_within_device),
  CONSTRAINT pin_app_class_net_id_key UNIQUE (app_class, net_id),
  CONSTRAINT pin_mode_check CHECK (mode = 'in' or mode = 'out'),
  CONSTRAINT pin_pud_check CHECK (pud = 'off' or pud = 'up' or pud = 'down')
)
WITH (
  OIDS=FALSE
);
