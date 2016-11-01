
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
 CONSTRAINT config_pkey PRIMARY KEY (app_class)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE gwu74.device
(
  app_class character varying(32) NOT NULL,
  addr integer NOT NULL,-- to get chip address in hex format (for Banana Pi): i2cdetect -y -a 2; convert it to decimal and put here 
  CONSTRAINT device_pkey PRIMARY KEY (app_class, addr)
)
WITH (
  OIDS=FALSE
);
--default configuration is based on device table
--for each pin, by default:
--id: pin numeration begins from the least device address
--mode=out
--o_mode=cmn
--if you need another configuration for some pins, put it here
CREATE TABLE gwu74.pin
(
  app_class character varying(32) NOT NULL,
  id integer NOT NULL, --each device has 8 pins, pin numeration begins from the least device address
  mode character varying (32) NOT NULL, --in || out
  pwm_period integer NOT NULL,
  CONSTRAINT pin_pkey PRIMARY KEY (app_class, id)
)
WITH (
  OIDS=FALSE
);

--you can assign pin_id to id(id is used for remote control), group_id leave blank
CREATE TABLE gwu74.id_pin_group
(
  app_class character varying(32) NOT NULL,
  id integer NOT NULL,--this id will be used for interprocess communication
  pin_id integer,
  group_id integer, 
  CONSTRAINT id_pin_group_pkey PRIMARY KEY (app_class, id)
)
WITH (
  OIDS=FALSE
);