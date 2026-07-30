# Alpha doc (fixture)

Links beta: see [`beta-doc.md`](beta-doc.md).

Mentions gamma-doc in prose with no link at all — EXPECT: unlinked mention.

A mention inside a fence must be IGNORED (the doc named below is named nowhere else in prose):

```
delta-doc is named here but fenced, so it must not be reported
```

Also links the hub: [`STATUS.md`](STATUS.md) — EXPECT: no missing-backlink finding (hub exempt).
