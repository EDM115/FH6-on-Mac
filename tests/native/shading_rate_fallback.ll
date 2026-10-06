; Authored minimal shader from shading_rate_repro.mm, edited to use 1x1 shading.
; No FH6 shader data. The original DXC comment summary no longer described this edit.
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @main() {
  %1 = add i32 0, 0  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %2 = icmp eq i32 %1, 0
  %3 = select i1 %2, float 1.000000e+00, float 0.000000e+00
  %4 = select i1 %2, float 0.000000e+00, float 1.000000e+00
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %3)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %4)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float 0.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 1.000000e+00)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.viewIdState = !{!4}
!dx.entryPoints = !{!5}

!0 = !{!"dxc(private) 1.9.0.0 (private, 00000000)"}
!1 = !{i32 1, i32 4}
!2 = !{i32 1, i32 10}
!3 = !{!"ps", i32 6, i32 4}
!4 = !{[3 x i32] [i32 1, i32 4, i32 3]}
!5 = !{void ()* @main, !"main", !6, null, !14}
!6 = !{null, !11, null}
!7 = !{!8}
!8 = !{i32 0, !"SV_ShadingRate", i8 5, i8 29, !9, i8 1, i32 1, i8 1, i32 0, i8 0, !10}
!9 = !{i32 0}
!10 = !{i32 3, i32 1}
!11 = !{!12}
!12 = !{i32 0, !"SV_Target", i8 9, i8 16, !9, i8 0, i32 1, i8 4, i32 0, i8 0, !13}
!13 = !{i32 3, i32 15}
!14 = !{i32 0, i64 0}
