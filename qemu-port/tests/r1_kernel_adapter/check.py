#!/usr/bin/env python3
"""Source-contract regressions only; does not execute kernel or model refcounts."""
from pathlib import Path
import hashlib
import json

root=Path(__file__).resolve().parents[3]
obj=(root/'qemu-port/variant/r1_g_objects.cpp').read_text()
boot=(root/'qemu-port/variant/r1_g_bootstrap.cpp').read_text()
checks={
    'genuine_process_create':'Kern::ProcessCreate(p,s->info,command,0)' in obj,
    'command_transfer_null_contract':'delete command;' in obj,
    'genuine_segment_create':'M::NewCodeSeg(s->info)' in obj and 'code->Create(s->info,0)' in obj,
    'genuine_process_loaded':'s->process->Loaded(s->info)' in obj,
    'genuine_segment_loaded':'s->code->Loaded(s->info)' in obj,
    'explicit_edge_hold':'b->code->CheckedOpen()' in obj and 'iEdges[n-1].exporter].code->Close()' in obj,
    'stillborn_before_release':obj.index('t->Stillborn()')<obj.index('s->process=0;p->Release()'),
    'publication_opens_not_consumes':'iCaller->MakeHandleAndOpen(EOwnerThread,s->process,h)' in obj,
    'security_sid_vid':'i.iS.iSecureId=im.secureId;i.iS.iVendorId=im.vendorId' in obj,
    'security_caps':'i.iS.iCaps[0]=im.capabilities[0];i.iS.iCaps[1]=im.capabilities[1]' in obj,
    'data_template_write':'s->info.iCodeLoadAddress:s->info.iDataLoadAddress' in obj,
    'entry_copyback_no_double_base':'i.iEntryPtVeneer,i.iExportDir' in obj,
    'dynamic_explicitly_unsupported':'Objects::PublishLibrary' in obj and 'return KErrNotSupported;}' in obj,
    'no_resume':'->Resume(' not in obj+boot,
    'no_legacy_parser':'E32Run' not in obj+boot and 'Crc32' not in obj+boot,
    'bounded_heap_work':'Kern::AllocZ(sizeof(Work))' in boot,
    'zero_cap_root':'w->plan.nodes[0].image.capabilities[0] || w->plan.nodes[0].image.capabilities[1]' in boot,
    'trusted_manifest_namespace':'const char* prefix="\\\\sys\\\\bin\\\\"' in boot,
    'suspended_handle_result':'out.process=w->objects.PublishedProcess()' in boot,
}
print(json.dumps(checks,indent=2))
assert all(checks.values())
print('PASS',len(checks),'source-contract regressions (not runtime tests)')
