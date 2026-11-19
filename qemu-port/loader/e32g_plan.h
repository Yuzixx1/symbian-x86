#ifndef E32G_PLAN_H
#define E32G_PLAN_H
#include "r1_image_source.h"

namespace R1G {
struct ClientContext { int32_t threadHandle, processHandle; uint32_t ownerType; };
enum LoadKind { ProcessLoad, LibraryLoad };
struct LoadRequest {
    ImageName name; ImageId requested; ClientContext client;
    Security clientSecurity; uint32_t requestedSecureId, minStack;
    LoadKind kind;
};
struct Limits {
    uint32_t imageBytes, aggregateBytes, candidates;
};
// Storage is caller-owned, disjoint, aligned for its declared element type.
// Nodes own immutable snapshot tokens while plan.source != 0. Never copy a live
// Plan/Node, mutate a Ready plan, or concurrently use a source/plan transaction.
struct Node {
    Snapshot snapshot; E32G::Image image; Path path;
    uint32_t firstEdge, edgeCount, state;
};
struct Edge { uint32_t importer, exporter, importIndex; };
enum PlanState { EmptyPlan, BuildingPlan, ReadyPlan };
struct Plan {
    ImageSource* source; Node* nodes; Edge* edges; uint32_t* order;
    uint32_t nodeCapacity, edgeCapacity, orderCapacity;
    uint32_t nodeCount, edgeCount, orderCount, byteCount;
    PlanState state; LoadRequest request; Limits limits;
};
struct Diagnostic { Result result; uint32_t node, detail; E32G::Error format; };

// Call once on fresh storage. No allocation, no automatic cleanup/destructor.
void InitPlan(Plan&, Node*, uint32_t nodes, Edge*, uint32_t edges,
              uint32_t* dependencyOrder, uint32_t orderCapacity);
// Build accepts EmptyPlan only. Any failure closes all snapshots and restores
// EmptyPlan. requested.version: ffffffff unspecified, fffffffe wildcard;
// finite values require same major and candidate minor >= request.
Result BuildPlan(Plan&, ImageSource&, const LoadRequest&, const Limits&, Diagnostic&);
// Idempotent, non-failing. Releases every retained snapshot once. Also called by
// Materialise on both success/failure; standalone Ready plans must be released.
void ReleasePlan(Plan&);
Security ImageSecurity(const E32G::Image&);
} // namespace R1G
#endif
