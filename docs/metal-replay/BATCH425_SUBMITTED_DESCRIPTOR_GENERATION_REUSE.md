# B425：提交后的 GPU 表槽位 generation 复用

真实 UE buffer24/offset3336：generation6459 在chunk35231退役，generation6609在38419重新分配、38420写入完整24B。97fcc796之前的拒绝来自 borrowedSlots 按offset永久累计；diagnostic已明确 prior table consumers18全部 committed，written(key)=0、opaque(table)=0，新slot live/empty。

沿用既有 RenderDoc submit与logical descriptor shadow机制，coverage65 在严格新generation的空allocation成功后，仅当旧slot已dead且整表旧borrowers全部committed、无opaque GPU table write，才清除旧key的borrowed/written/fresh标记。后续CPU value/binding仍必须既有typed24B来源、generation、range完整校验，ReplayCPUBufferUpdate在CPU写Native表前等待已经committed的提交。allocation本身不改变Native memory。没有允许未提交消费者复用。

精确97fcc796（metal-submitted-gpu-reuse.dWBaKp）：6captures/24resetseeks，108 indirect+24fresh+24mixed+24retirement+24reuse API/CLI malformed groups。第一提交原Native GPU descriptor producer、compute、MRT、resolve和retirement；capture通过bounded status polling等待Native完成，没有录入第一CB显式wait chunk。随后新generation full24CPU write/sourcebinding、第二CB原Nativecompute消费正确packet/result。每次partial/full/reset checks及retired bytes保持通过；missing retire/stale generation/unsubmitted consumer/unknown consumer/shortvalue/missingsource/wrongsource拒绝。

实际UE新generation value通过，下一阻塞是current-frame drawable17885没有initialpixels时绑定SRV（B426）。当前真实frame仍未initial upload或GPU replay；UI未验收，无提交推送。
