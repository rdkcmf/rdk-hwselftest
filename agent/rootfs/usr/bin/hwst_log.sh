# hwst_log - log message using selected logging means
# args:
#   $1: message to log
#   $2: log level
#                  1 - normal operation log
#                  2 - debug log
#   $2: log target (0 - systemd journal, 1 - direct)
#
function hwst_log() {
    msg=$1
    lvl=$2
    target=$3

    time="|`/bin/date "+%Y-%m-%d %H:%M:%S"`"

    case "$lvl" in
        1)
            mark="HWST_LOG"
            file=/opt/logs/hwselftest.log
            ;;
        2)
            mark="HWST_DBG"
            file=/opt/logs/hwselftest.dbg
            ;;
        *)
            mark="HWST_UNK"
            ;;
    esac

    if [ "$mark" != "HWST_UNK" ]; then
        if [ $target -ne 0 ]; then
            echo $mark $time $msg >> $file
        else
            echo $mark $time $msg
        fi
    fi
}

# current wrappers for log both write to stdout,
# so that systemd journal is used to confirm with
# lightsleep demands

# normal operation log to stdout
function n_log() {
    hwst_log "$1" 1 0
}

# debug log to stdout
function d_log() {
    hwst_log "$1" 2 0
}

