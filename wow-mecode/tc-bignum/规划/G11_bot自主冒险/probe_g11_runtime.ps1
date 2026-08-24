# DEPRECATED / 已停用
#
# G11 runtime probe v1 used PowerShell's shared formatting pipeline. The report
# could hide object columns (including config values and log Line content), so
# it is not reliable for acceptance decisions. Fail closed instead of emitting
# another ambiguous report.
#
# Use the self-tested replacement:
#   probe_g11_runtime_v2.py

Write-Error "This probe is deprecated because its report formatting can hide fields. Use probe_g11_runtime_v2.py."
exit 2
