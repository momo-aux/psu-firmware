/*
 * EEZ PSU Firmware
 * Copyright (C) 2015 Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "psu.h"
#include "gui_data.h"

#include "channel.h"

#include "gui_view.h"

namespace eez {
namespace psu {
namespace gui {
namespace data {

static Channel *selected_channel;

struct ChannelStateFlags {
    unsigned mode : 2;
    unsigned state : 2;
    unsigned ovp : 2;
    unsigned ocp : 2;
    unsigned opp : 2;
    unsigned otp : 2;
    unsigned dp : 2;
};

struct ChannelState {
    char mon_value[8];
    char u_set[8];
    char i_set[8];
    ChannelStateFlags flags;
};


static ChannelState channel_last_state[CH_NUM];
static ChannelState *selected_channel_last_state;

int count(uint16_t id) {
    if (id == DATA_ID_CHANNELS) {
        return CH_NUM;
    }
    return 0;
}

void select(uint16_t id, int index) {
    if (id == DATA_ID_CHANNELS) {
        selected_channel = &Channel::get(index);
        selected_channel_last_state = channel_last_state + index;
    }
}

char *get(uint16_t id, bool &changed) {
    static char value[8];

    changed = false;

    if (id == DATA_ID_OUTPUT_STATE) {
        uint8_t value = selected_channel->isOutputEnabled() ? 1 : 0;
        if (value != selected_channel_last_state->flags.state) {
            selected_channel_last_state->flags.state = value;
            changed = true;
        }
        return (char *)value;
    }
    
    if (id == DATA_ID_OUTPUT_MODE) {
        char *mode_str = selected_channel->getCvModeStr();
        uint8_t value = strcmp(mode_str, "UR") == 0 ? 1 : 0;
        if (value != selected_channel_last_state->flags.mode) {
            selected_channel_last_state->flags.mode = value;
            changed = true;
        }
        return (char *)value;
    }
    
    if (id == DATA_ID_MON_VALUE) {
        value[0] = 0;
        char *mode_str = selected_channel->getCvModeStr();
        if (strcmp(mode_str, "CC") != 0) {
            // CC -> volt
            util::strcatVoltage(value, selected_channel->u.mon);
        } else if (strcmp(mode_str, "CV") != 0) {
            // CV -> curr
            util::strcatCurrent(value, selected_channel->i.mon);
        } else {
            // UR ->
            if (selected_channel->u.mon < selected_channel->i.mon) {
                // min(volt, curr)
                util::strcatVoltage(value, selected_channel->u.mon);
            } else {
                // or curr if equal
                util::strcatCurrent(value, selected_channel->i.mon);
            }
        }
        if (strcmp(selected_channel_last_state->mon_value, value) != 0) {
            strcpy(selected_channel_last_state->mon_value, value);
            changed = true;
        }
        return value;
    }
    
    if (id == DATA_ID_VOLT) {
        value[0] = 0;
        util::strcatVoltage(value, selected_channel->u.set);
        if (strcmp(selected_channel_last_state->u_set, value) == 0) {
            strcpy(selected_channel_last_state->u_set, value);
            changed = true;
        }
        return value;
    }
    
    if (id == DATA_ID_CURR) {
        value[0] = 0;
        util::strcatCurrent(value, selected_channel->i.set);
        if (strcmp(selected_channel_last_state->i_set, value) != 0) {
            strcpy(selected_channel_last_state->i_set, value);
            changed = true;
        }
        return value;
    }
    
    if (id == DATA_ID_OVP) {
        uint8_t value;
        if (!selected_channel->prot_conf.flags.u_state) value = 1;
        else if (!selected_channel->ovp.flags.tripped) value = 2;
        else value = 3;
        if (value != selected_channel_last_state->flags.ovp) {
            selected_channel_last_state->flags.ovp = value;
            changed = true;
        }
        return (char *)value;
    }
    
    if (id == DATA_ID_OCP) {
        uint8_t value;
        if (!selected_channel->prot_conf.flags.i_state) value = 1;
        else if (!selected_channel->ocp.flags.tripped) value = 2;
        else value = 3;
        if (value != selected_channel_last_state->flags.ocp) {
            selected_channel_last_state->flags.ocp = value;
            changed = true;
        }
        return (char *)value;
    }
    
    if (id == DATA_ID_OPP) {
        uint8_t value;
        if (!selected_channel->prot_conf.flags.p_state) value = 1;
        else if (!selected_channel->opp.flags.tripped) value = 2;
        else value = 3;
        if (value != selected_channel_last_state->flags.opp) {
            selected_channel_last_state->flags.opp = value;
            changed = true;
        }
        return (char *)value;
    }
    
    if (id == DATA_ID_OTP) {
        uint8_t value;
        if (!temperature::prot_conf[temp_sensor::MAIN].state) value = 1;
        else if (!temperature::isSensorTripped(temp_sensor::MAIN)) value = 2;
        else value = 3;
        if (value != selected_channel_last_state->flags.otp) {
            selected_channel_last_state->flags.otp = value;
            changed = true;
        }
        return (char *)value;
    } 
    
    if (id == DATA_ID_DP) {
        uint8_t value = selected_channel->flags.dp_on ? 1 : 2;
        if (value != selected_channel_last_state->flags.dp) {
            selected_channel_last_state->flags.dp = value;
            changed = true;
        }
        return (char *)value;
    }

    return 0;
}

void set(uint16_t id, const char *value) {
    if (id == DATA_ID_VOLT) {
    } else if (id == DATA_ID_CURR) {
    }
}

}
}
}
} // namespace eez::psu::ui::data