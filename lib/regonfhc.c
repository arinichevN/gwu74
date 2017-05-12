
#include "regonfhc.h"
#include "reg.c"

void regonfhc_onf(RegOnfHC *item) {
    switch (item->state) {
        case REG_INIT:
            item->tmr.ready = 0;
            item->state_r = REG_HEATER;
            reg_controlEM(&item->heater.em, 0.0f);
            reg_controlEM(&item->cooler.em, 0.0f);
            item->heater.output = 0.0f;
            item->cooler.output = 0.0f;
            item->output = 0.0f;
            item->snsrf_count = 0;
            item->state_onf = REG_WAIT;
            if (acp_readSensorFTS(&item->sensor)) {
                if (SNSR_VAL > item->goal) {
                    item->state_r = REG_COOLER;
                }
                item->state = REG_BUSY;
            }
            break;
        case REG_BUSY:
        {
            if (acp_readSensorFTS(&item->sensor)) {
                item->snsrf_count = 0;
                int value_is_out = 0;
                char other_em;
                EM *em_turn_off;
                switch (item->state_r) {
                    case REG_HEATER:
                        if (VAL_IS_OUT_H) {
                            value_is_out = 1;
                        }
                        other_em = REG_COOLER;
                        em_turn_off = &item->heater.em;
                        break;
                    case REG_COOLER:
                        if (VAL_IS_OUT_C) {
                            value_is_out = 1;
                        }
                        other_em = REG_HEATER;
                        em_turn_off = &item->cooler.em;
                        break;
                }
                if (value_is_out) {
                    if (ton_ts(item->change_gap, &item->tmr)) {
#ifdef MODE_DEBUG
                        char *state1 = reg_getStateStr(item->state_r);
                        char *state2 = reg_getStateStr(other_em);
                        printf("state_r switched from %s to %s\n", state1, state2);
#endif
                        item->state_r = other_em;
                        reg_controlEM(em_turn_off, 0.0f);
                        if (em_turn_off == &item->heater.em) {
                            item->heater.output = 0.0f;
                        } else if (em_turn_off == &item->cooler.em) {
                            item->cooler.output = 0.0f;
                        }
                    }
                } else {
                    item->tmr.ready = 0;
                }
                EM *em = NULL;
                EM *em_other = NULL;
                switch (item->state_r) {
                    case REG_HEATER:
                        em = &item->heater.em;
                        em_other = &item->cooler.em;
                        break;
                    case REG_COOLER:
                        em = &item->cooler.em;
                        em_other = &item->heater.em;
                        break;
                }
                switch (item->state_onf) {
                    case REG_DO:
                        switch (item->state_r) {
                            case REG_HEATER:
                                if (SNSR_VAL > item->goal + item->heater.delta) {
                                    item->state_onf = REG_WAIT;
                                }
                                break;
                            case REG_COOLER:
                                if (SNSR_VAL < item->goal - item->cooler.delta) {
                                    item->state_onf = REG_WAIT;
                                }
                                break;
                        }
                        item->output = em->pwm_rsl;
                        break;
                    case REG_WAIT:
                        switch (item->state_r) {
                            case REG_HEATER:
                                if (SNSR_VAL < item->goal - item->heater.delta) {
                                    item->state_onf = REG_DO;
                                }
                                break;
                            case REG_COOLER:
                                if (SNSR_VAL > item->goal + item->cooler.delta) {
                                    item->state_onf = REG_DO;
                                }
                                break;
                        }
                        item->output = 0.0f;
                        break;
                }
                reg_controlEM(em, item->output);
                reg_controlEM(em_other, 0.0f);
                if (em == &item->heater.em) {
                    item->heater.output = item->output;
                } else if (em == &item->cooler.em) {
                    item->cooler.output = item->output;
                }
                if (em_other == &item->heater.em) {
                    item->heater.output = 0.0f;
                } else if (em_other == &item->cooler.em) {
                    item->cooler.output = 0.0f;
                }
            } else {
                if (item->snsrf_count > SNSRF_COUNT_MAX) {
                    reg_controlEM(&item->heater.em, 0.0f);
                    reg_controlEM(&item->cooler.em, 0.0f);
                    item->heater.output = 0.0f;
                    item->cooler.output = 0.0f;
                    item->output = 0.0f;
                    item->state = REG_INIT;
#ifdef MODE_DEBUG
                    puts("reading from sensor failed, EM turned off");
#endif
                } else {
                    item->snsrf_count++;
#ifdef MODE_DEBUG
                    printf("sensor failure counter: %d\n", item->snsrf_count);
#endif
                }
            }
            break;
        }
        case REG_DISABLE:
            reg_controlEM(&item->heater.em, 0.0f);
            reg_controlEM(&item->cooler.em, 0.0f);
            item->heater.output = 0.0f;
            item->cooler.output = 0.0f;
            item->output = 0.0f;
            item->tmr.ready = 0;
            item->state_r = REG_OFF;
            item->state_onf = REG_OFF;
            item->state = REG_OFF;
            break;
        case REG_OFF:
            break;
        default:
            item->state = REG_INIT;
            break;
    }
#ifdef MODE_DEBUG
    char *state = reg_getStateStr(item->state);
    char *state_r = reg_getStateStr(item->state_r);
    char *state_onf = reg_getStateStr(item->state_onf);
    struct timespec tm1 = getTimeRestTmr(item->change_gap, item->tmr);
    printf("state=%s state_onf=%s EM_state=%s goal=%.1f delta_h=%.1f delta_c=%.1f real=%.1f real_st=%d out=%.1f change_time=%ldsec\n", state, state_onf, state_r, item->goal, item->heater.delta, item->cooler.delta, SNSR_VAL,item->sensor.value.state, item->output, tm1.tv_sec);
#endif
}

void regonfhc_enable(RegOnfHC *item) {
    item->state = REG_INIT;
}

void regonfhc_disable(RegOnfHC *item) {
    item->state = REG_DISABLE;
}

void regonfhc_setCoolerDelta(RegOnfHC *item, float value) {
    item->cooler.delta = value;
    item->state = REG_INIT;
}

void regonfhc_setHeaterDelta(RegOnfHC *item, float value) {
    item->heater.delta = value;
    item->state = REG_INIT;
}

void regonfhc_setGoal(RegOnfHC *item, float value) {
    item->goal = value;
    item->state = REG_INIT;
}

void regonfhc_setChangeGap(RegOnfHC *item, int value) {
    item->change_gap.tv_sec = value;
    item->change_gap.tv_nsec = 0;
}

void regonfhc_setHeaterPower(RegOnfHC *item, float value) {
    reg_controlEM(&item->heater.em, value);
    item->heater.output = value;
    item->output = item->heater.output;
}

void regonfhc_setCoolerPower(RegOnfHC *item, float value) {
    reg_controlEM(&item->cooler.em, value);
    item->cooler.output = value;
    item->output = item->cooler.output;
}

void regonfhc_turnOff(RegOnfHC *item) {
    item->state = REG_OFF;
    reg_controlEM(&item->cooler.em, 0.0f);
    reg_controlEM(&item->heater.em, 0.0f);
}