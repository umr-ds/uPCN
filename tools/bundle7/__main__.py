from datetime import timedelta
from . import *

primary_block = PrimaryBlock(
    bundle_proc_flags=BundleProcFlag.MUST_NOT_BE_FRAGMENTED
        | BundleProcFlag.CONTAINS_MANIFEST
        | BundleProcFlag.REPORT_DELIVERY
        | BundleProcFlag.REPORT_STATUS_TIME,
    destination="dtn:GS2",
    source="dtn:none",
    report_to="dtn:none",
    creation_time=CreationTimestamp(dtn_start, 0),
)

payload = PayloadBlock(b"Hello world!")

# Extension blocks
previous_node = PreviousNodeBlock("dtn:GS4")
hop_count     = HopCountBlock(30, 0)
bundle_age    = BundleAgeBlock(0)

bundle = Bundle([
    primary_block,
    previous_node,
    hop_count,
    bundle_age,
    payload,
])

# Bundle Status Report "Received bundle"
status_report = BundleStatusReport(infos=StatusCode.RECEIVED_BUNDLE,
                                   reason=ReasonCode.NO_INFO,
                                   bundle=bundle,
                                   time=dtn_start + timedelta(seconds=10))

status_report_bundle = Bundle([
    PrimaryBlock(
        bundle_proc_flags=BundleProcFlag.ADMINISTRATIVE_RECORD,
        destination=bundle.primary_block.report_to,
    ),
    status_report,
])

print_hex(bundle.serialize())
# print_hex(status_report_bundle.serialize())
# print_hex(status_report.serialize())

with open("bundle.cbor", "wb") as fd:
    fd.write(bundle.serialize())
