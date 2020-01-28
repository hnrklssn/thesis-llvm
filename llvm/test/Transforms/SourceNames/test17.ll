; RUN: opt -load LLVMHello.so -source-names < %s 2>&1 >&2 | FileCheck %s 

; CHECK:  %inner2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %arr, i64 0, i32 1, i64 %indvars.iv43, i32 
; CHECK:1, !dbg !47 --> arr->flexibleArr[i].inner2                                                                       
; CHECK:  %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %0, i64 0, i32 0, !dbg !47 --> arr->fle
; CHECK:xibleArr[i].inner2->firstField                                                                                   
; CHECK:  %firstField16 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %arr, i64 %indvars.iv43, i32 0, !dbg 
; CHECK:!47 --> arr[i].firstField                                                                                        
; CHECK:  %arrayidx7 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %arr, i64 0, i32 1, i64 %indvars.iv43, i
; CHECK:32 2, i64 %indvars.iv, !dbg !61 --> arr->flexibleArr[i].nameBuf[j]                                               
; CHECK:---args section--
; CHECK:print variable arr
; CHECK:print variable n

; ModuleID = '../test.c'
source_filename = "../test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.MyStruct = type { i32, [0 x %struct.Inner] }
%struct.Inner = type { i8*, %struct.MyStruct*, [10 x i8] }

; Function Attrs: norecurse nounwind optsize readonly uwtable
define dso_local i32 @my_fun(%struct.MyStruct* nocapture readonly %arr, i32 %n) local_unnamed_addr #0 !dbg !7 {
entry:
  call void @llvm.dbg.value(metadata %struct.MyStruct* %arr, metadata !30, metadata !DIExpression()), !dbg !42
  call void @llvm.dbg.value(metadata i32 %n, metadata !31, metadata !DIExpression()), !dbg !42
  call void @llvm.dbg.value(metadata i32 0, metadata !32, metadata !DIExpression()), !dbg !42
  call void @llvm.dbg.value(metadata i32 0, metadata !33, metadata !DIExpression()), !dbg !43
  %cmp38 = icmp sgt i32 %n, 0, !dbg !44
  br i1 %cmp38, label %for.cond1.preheader.lr.ph, label %for.cond.cleanup, !dbg !45

for.cond1.preheader.lr.ph:                        ; preds = %entry
  %wide.trip.count45 = zext i32 %n to i64, !dbg !44
  br label %for.body4.lr.ph, !dbg !45

for.body4.lr.ph:                                  ; preds = %for.cond1.preheader.lr.ph, %for.cond.cleanup3
  %indvars.iv43 = phi i64 [ 0, %for.cond1.preheader.lr.ph ], [ %indvars.iv.next44, %for.cond.cleanup3 ]
  %secret_int.041 = phi i32 [ 0, %for.cond1.preheader.lr.ph ], [ %add18, %for.cond.cleanup3 ]
  call void @llvm.dbg.value(metadata i32 %secret_int.041, metadata !32, metadata !DIExpression()), !dbg !42
  call void @llvm.dbg.value(metadata i64 %indvars.iv43, metadata !33, metadata !DIExpression()), !dbg !43
  call void @llvm.dbg.value(metadata i32 %secret_int.041, metadata !32, metadata !DIExpression()), !dbg !42
  call void @llvm.dbg.value(metadata i32 0, metadata !35, metadata !DIExpression()), !dbg !46
  %inner2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %arr, i64 0, i32 1, i64 %indvars.iv43, i32 1, !dbg !47
  %0 = load %struct.MyStruct*, %struct.MyStruct** %inner2, align 8, !dbg !47, !tbaa !48
  %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %0, i64 0, i32 0, !dbg !47
  %1 = load i32, i32* %firstField, align 8, !dbg !47, !tbaa !53
  %sext = shl i32 %1, 24, !dbg !47
  %conv13 = ashr exact i32 %sext, 24, !dbg !47
  %firstField16 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %arr, i64 %indvars.iv43, i32 0, !dbg !47
  %2 = load i32, i32* %firstField16, align 8, !dbg !47, !tbaa !53
  br label %for.body4, !dbg !55

for.cond.cleanup:                                 ; preds = %for.cond.cleanup3, %entry
  %secret_int.0.lcssa = phi i32 [ 0, %entry ], [ %add18, %for.cond.cleanup3 ], !dbg !56
  call void @llvm.dbg.value(metadata i32 %secret_int.0.lcssa, metadata !32, metadata !DIExpression()), !dbg !42
  ret i32 %secret_int.0.lcssa, !dbg !57

for.cond.cleanup3:                                ; preds = %for.body4
  %indvars.iv.next44 = add nuw nsw i64 %indvars.iv43, 1, !dbg !58
  call void @llvm.dbg.value(metadata i32 %add18, metadata !32, metadata !DIExpression()), !dbg !42
  call void @llvm.dbg.value(metadata i64 %indvars.iv.next44, metadata !33, metadata !DIExpression()), !dbg !43
  %exitcond46 = icmp eq i64 %indvars.iv.next44, %wide.trip.count45, !dbg !44
  br i1 %exitcond46, label %for.cond.cleanup, label %for.body4.lr.ph, !dbg !45, !llvm.loop !59

for.body4:                                        ; preds = %for.body4, %for.body4.lr.ph
  %indvars.iv = phi i64 [ 0, %for.body4.lr.ph ], [ %indvars.iv.next, %for.body4 ]
  %secret_int.137 = phi i32 [ %secret_int.041, %for.body4.lr.ph ], [ %add18, %for.body4 ]
  call void @llvm.dbg.value(metadata i32 %secret_int.137, metadata !32, metadata !DIExpression()), !dbg !42
  call void @llvm.dbg.value(metadata i64 %indvars.iv, metadata !35, metadata !DIExpression()), !dbg !46
  %arrayidx7 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %arr, i64 0, i32 1, i64 %indvars.iv43, i32 2, i64 %indvars.iv, !dbg !61
  %3 = load i8, i8* %arrayidx7, align 1, !dbg !61, !tbaa !62
  call void @llvm.dbg.value(metadata i8 %3, metadata !38, metadata !DIExpression()), !dbg !47
  call void @llvm.dbg.value(metadata i8 undef, metadata !41, metadata !DIExpression()), !dbg !47
  %conv12 = sext i8 %3 to i32, !dbg !63
  %add = add i32 %secret_int.137, %conv12, !dbg !64
  %add17 = add i32 %add, %2, !dbg !65
  %add18 = add i32 %add17, %conv13, !dbg !66
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1, !dbg !67
  call void @llvm.dbg.value(metadata i32 %add18, metadata !32, metadata !DIExpression()), !dbg !42
  call void @llvm.dbg.value(metadata i64 %indvars.iv.next, metadata !35, metadata !DIExpression()), !dbg !46
  %exitcond = icmp eq i64 %indvars.iv.next, %wide.trip.count45, !dbg !68
  br i1 %exitcond, label %for.cond.cleanup3, label %for.body4, !dbg !55, !llvm.loop !69
}

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

attributes #0 = { norecurse nounwind optsize readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable willreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 10.0.0 (git@github.com:hnrklssn/thesis-llvm.git 88e2db1a0042bbed5d8a689216b0b37dd3601530)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, nameTableKind: None)
!1 = !DIFile(filename: "../test.c", directory: "/home/dat14hol/git/llvm-project/build")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 10.0.0 (git@github.com:hnrklssn/thesis-llvm.git 88e2db1a0042bbed5d8a689216b0b37dd3601530)"}
!7 = distinct !DISubprogram(name: "my_fun", scope: !1, file: !1, line: 14, type: !8, scopeLine: 14, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !29)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !11, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !12, size: 64)
!12 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "MyStruct", file: !1, line: 7, size: 64, elements: !13)
!13 = !{!14, !15}
!14 = !DIDerivedType(tag: DW_TAG_member, name: "firstField", scope: !12, file: !1, line: 8, baseType: !10, size: 32)
!15 = !DIDerivedType(tag: DW_TAG_member, name: "flexibleArr", scope: !12, file: !1, line: 9, baseType: !16, offset: 64)
!16 = !DICompositeType(tag: DW_TAG_array_type, baseType: !17, elements: !27)
!17 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Inner", file: !1, line: 2, size: 256, elements: !18)
!18 = !{!19, !21, !22}
!19 = !DIDerivedType(tag: DW_TAG_member, name: "inner1", scope: !17, file: !1, line: 3, baseType: !20, size: 64)
!20 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!21 = !DIDerivedType(tag: DW_TAG_member, name: "inner2", scope: !17, file: !1, line: 4, baseType: !11, size: 64, offset: 64)
!22 = !DIDerivedType(tag: DW_TAG_member, name: "nameBuf", scope: !17, file: !1, line: 5, baseType: !23, size: 80, offset: 128)
!23 = !DICompositeType(tag: DW_TAG_array_type, baseType: !24, size: 80, elements: !25)
!24 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!25 = !{!26}
!26 = !DISubrange(count: 10)
!27 = !{!28}
!28 = !DISubrange(count: -1)
!29 = !{!30, !31, !32, !33, !35, !38, !41}
!30 = !DILocalVariable(name: "arr", arg: 1, scope: !7, file: !1, line: 14, type: !11)
!31 = !DILocalVariable(name: "n", arg: 2, scope: !7, file: !1, line: 14, type: !10)
!32 = !DILocalVariable(name: "secret_int", scope: !7, file: !1, line: 15, type: !10)
!33 = !DILocalVariable(name: "i", scope: !34, file: !1, line: 16, type: !10)
!34 = distinct !DILexicalBlock(scope: !7, file: !1, line: 16, column: 3)
!35 = !DILocalVariable(name: "j", scope: !36, file: !1, line: 17, type: !10)
!36 = distinct !DILexicalBlock(scope: !37, file: !1, line: 17, column: 5)
!37 = distinct !DILexicalBlock(scope: !34, file: !1, line: 16, column: 3)
!38 = !DILocalVariable(name: "tmp", scope: !39, file: !1, line: 18, type: !24)
!39 = distinct !DILexicalBlock(scope: !40, file: !1, line: 17, column: 33)
!40 = distinct !DILexicalBlock(scope: !36, file: !1, line: 17, column: 5)
!41 = !DILocalVariable(name: "tmp2", scope: !39, file: !1, line: 19, type: !24)
!42 = !DILocation(line: 0, scope: !7)
!43 = !DILocation(line: 0, scope: !34)
!44 = !DILocation(line: 16, column: 21, scope: !37)
!45 = !DILocation(line: 16, column: 3, scope: !34)
!46 = !DILocation(line: 0, scope: !36)
!47 = !DILocation(line: 0, scope: !39)
!48 = !{!49, !50, i64 8}
!49 = !{!"Inner", !50, i64 0, !50, i64 8, !51, i64 16}
!50 = !{!"any pointer", !51, i64 0}
!51 = !{!"omnipotent char", !52, i64 0}
!52 = !{!"Simple C/C++ TBAA"}
!53 = !{!54, !54, i64 0}
!54 = !{!"int", !51, i64 0}
!55 = !DILocation(line: 17, column: 5, scope: !36)
!56 = !DILocation(line: 15, column: 6, scope: !7)
!57 = !DILocation(line: 22, column: 2, scope: !7)
!58 = !DILocation(line: 16, column: 27, scope: !37)
!59 = distinct !{!59, !45, !60}
!60 = !DILocation(line: 21, column: 5, scope: !34)
!61 = !DILocation(line: 18, column: 18, scope: !39)
!62 = !{!51, !51, i64 0}
!63 = !DILocation(line: 20, column: 21, scope: !39)
!64 = !DILocation(line: 20, column: 25, scope: !39)
!65 = !DILocation(line: 20, column: 32, scope: !39)
!66 = !DILocation(line: 20, column: 18, scope: !39)
!67 = !DILocation(line: 17, column: 29, scope: !40)
!68 = !DILocation(line: 17, column: 23, scope: !40)
!69 = distinct !{!69, !55, !70}
!70 = !DILocation(line: 21, column: 5, scope: !36)
