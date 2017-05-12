delete from device;
INSERT INTO device(id,addr_i2c) VALUES (1,32);

delete from pin;
INSERT INTO pin(net_id,device_id,id_within_device,mode,pud,rsl,pwm_period_sec,pwm_period_nsec) VALUES (1,1,0,'out','off',1000,10,0);
INSERT INTO pin(net_id,device_id,id_within_device,mode,pud,rsl,pwm_period_sec,pwm_period_nsec) VALUES (2,1,1,'out','off',1000,10,0);
INSERT INTO pin(net_id,device_id,id_within_device,mode,pud,rsl,pwm_period_sec,pwm_period_nsec) VALUES (3,1,2,'out','off',1000,10,0);
INSERT INTO pin(net_id,device_id,id_within_device,mode,pud,rsl,pwm_period_sec,pwm_period_nsec) VALUES (4,1,3,'out','off',1000,10,0);
INSERT INTO pin(net_id,device_id,id_within_device,mode,pud,rsl,pwm_period_sec,pwm_period_nsec) VALUES (5,1,4,'out','off',1000,10,0);
INSERT INTO pin(net_id,device_id,id_within_device,mode,pud,rsl,pwm_period_sec,pwm_period_nsec) VALUES (6,1,5,'out','off',1000,10,0);
INSERT INTO pin(net_id,device_id,id_within_device,mode,pud,rsl,pwm_period_sec,pwm_period_nsec) VALUES (7,1,6,'out','off',1000,10,0);
INSERT INTO pin(net_id,device_id,id_within_device,mode,pud,rsl,pwm_period_sec,pwm_period_nsec) VALUES (8,1,7,'out','off',1000,10,0);