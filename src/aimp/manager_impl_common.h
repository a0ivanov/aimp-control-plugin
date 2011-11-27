// Copyright (c) 2011, Alexey Ivanov

#ifndef AIMP_MANAGER_IMPL_COMMON_H
#define AIMP_MANAGER_IMPL_COMMON_H

namespace AIMPPlayer
{

inline const char* asString(AIMPManager::STATUS status)
{
 switch (status) {
    case AIMPManager::STATUS_VOLUME:    return "VOLUME";
    case AIMPManager::STATUS_BALANCE:   return "BALANCE";
    case AIMPManager::STATUS_SPEED:     return "SPEED";
    case AIMPManager::STATUS_Player:    return "Player";
    case AIMPManager::STATUS_MUTE:      return "MUTE";
    case AIMPManager::STATUS_REVERB:    return "REVERB";
    case AIMPManager::STATUS_ECHO:      return "ECHO";
    case AIMPManager::STATUS_CHORUS:    return "CHORUS";
    case AIMPManager::STATUS_Flanger:   return "Flanger";

    case AIMPManager::STATUS_EQ_STS:    return "EQ_STS";
    case AIMPManager::STATUS_EQ_SLDR01: return "EQ_SLDR01";
    case AIMPManager::STATUS_EQ_SLDR02: return "EQ_SLDR02";
    case AIMPManager::STATUS_EQ_SLDR03: return "EQ_SLDR03";
    case AIMPManager::STATUS_EQ_SLDR04: return "EQ_SLDR04";
    case AIMPManager::STATUS_EQ_SLDR05: return "EQ_SLDR05";
    case AIMPManager::STATUS_EQ_SLDR06: return "EQ_SLDR06";
    case AIMPManager::STATUS_EQ_SLDR07: return "EQ_SLDR07";
    case AIMPManager::STATUS_EQ_SLDR08: return "EQ_SLDR08";
    case AIMPManager::STATUS_EQ_SLDR09: return "EQ_SLDR09";
    case AIMPManager::STATUS_EQ_SLDR10: return "EQ_SLDR10";
    case AIMPManager::STATUS_EQ_SLDR11: return "EQ_SLDR11";
    case AIMPManager::STATUS_EQ_SLDR12: return "EQ_SLDR12";
    case AIMPManager::STATUS_EQ_SLDR13: return "EQ_SLDR13";
    case AIMPManager::STATUS_EQ_SLDR14: return "EQ_SLDR14";
    case AIMPManager::STATUS_EQ_SLDR15: return "EQ_SLDR15";
    case AIMPManager::STATUS_EQ_SLDR16: return "EQ_SLDR16";
    case AIMPManager::STATUS_EQ_SLDR17: return "EQ_SLDR17";
    case AIMPManager::STATUS_EQ_SLDR18: return "EQ_SLDR18";

    case AIMPManager::STATUS_REPEAT:    return "REPEAT";
    case AIMPManager::STATUS_ON_STOP:   return "ON_STOP";
    case AIMPManager::STATUS_POS:       return "POS";
    case AIMPManager::STATUS_LENGTH:    return "LENGTH";
    case AIMPManager::STATUS_REPEATPLS: return "REPEATPLS";
    case AIMPManager::STATUS_REP_PLS_1: return "REP_PLS_1";
    case AIMPManager::STATUS_KBPS:      return "KBPS";
    case AIMPManager::STATUS_KHZ:       return "KHZ";
    case AIMPManager::STATUS_MODE:      return "MODE";
    case AIMPManager::STATUS_RADIO:     return "RADIO";
    case AIMPManager::STATUS_STREAM_TYPE: return "STREAM_TYPE";
    case AIMPManager::STATUS_TIMER:     return "TIMER";
    case AIMPManager::STATUS_SHUFFLE:   return "SHUFFLE";
    case AIMPManager::STATUS_MAIN_HWND: return "MAIN_HWND";
    case AIMPManager::STATUS_TC_HWND:   return "TC_HWND";
    case AIMPManager::STATUS_APP_HWND:  return "APP_HWND";
    case AIMPManager::STATUS_PL_HWND:   return "PL_HWND";
    case AIMPManager::STATUS_EQ_HWND:   return "EQ_HWND";
    case AIMPManager::STATUS_TRAY:      return "TRAY";
    default:
        break;
    }

    assert(!"unknown status in "__FUNCTION__);
    static std::string status_string;
    std::stringstream os;
    os << static_cast<int>(status);
    status_string = os.str();
    return status_string.c_str();
}

} // namespace AIMPPlayer

#endif // #ifndef AIMP_MANAGER_IMPL_COMMON_H
