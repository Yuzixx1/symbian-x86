#ifndef E32G_LOAD_H
#define E32G_LOAD_H
#include "r1_object_adapter.h"

namespace R1G {
enum TransactionState { IdleTransaction, ActiveTransaction, CommittedTransaction,
                        AbortedTransaction };
struct Ledger {
    ObjectRecord* records; uint32_t capacity, count;
    TransactionState state; bool published;
};
struct LoadResult { int32_t clientHandle; uint32_t segments; };
void InitLedger(Ledger&, ObjectRecord*, uint32_t capacity);
// Consumes a ReadyPlan: releases its snapshots on success AND failure. Failure
// restores adapter live-reference baseline, no publication. Caller storage is
// reusable after InitLedger; committed handles belong to caller, never Abort.
Result Materialise(Plan&, LoaderObjects&, Ledger&, LoadResult&, Diagnostic&);
// Idempotent only for this ledger/adapter pair; committed graph is never killed.
void Abort(Ledger&, LoaderObjects&);
} // namespace R1G
#endif
