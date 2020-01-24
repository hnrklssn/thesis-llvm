; RUN: opt -load LLVMHello.so -source-names < %s 2>&1 >&2 | FileCheck %s 


; CHECK:  %inner1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 0, !dbg !32 --> s.secondField.inner1
; CHECK:  %inner2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 1, !dbg !43 --> s.secondField.inner2
; CHECK:  %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %1, i64 0, i32 0, !dbg !46 --> s.secondField.inner2->firstField
; CHECK:  %arrayidx = getelementptr inbounds i32, i32* %arr, i64 %idxprom12, !dbg !54 --> arr[n]
; CHECK:  %firstField2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 0, !dbg !56 --> s.firstField
; CHECK:---args section--
; CHECK:print variable s
; CHECK:print variable arr
; CHECK:print variable n

; ModuleID = '<stdin>'
source_filename = "../test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.MyStruct = type { i32, %struct.Inner }
%struct.Inner = type { i8*, %struct.MyStruct* }

; Function Attrs: norecurse nounwind readonly uwtable
define dso_local i32 @my_fun(%struct.MyStruct* nocapture readonly byval(%struct.MyStruct) align 8 %s, i32* nocapture readonly %arr, i32 %n) local_unnamed_addr #0 !dbg !7 {
entry:
  call void @llvm.dbg.declare(metadata %struct.MyStruct* %s, metadata !24, metadata !DIExpression()), !dbg !30
  call void @llvm.dbg.value(metadata i32* %arr, metadata !25, metadata !DIExpression()), !dbg !31
  call void @llvm.dbg.value(metadata i32 %n, metadata !26, metadata !DIExpression()), !dbg !31
  %inner1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 0, !dbg !32
  %0 = load i8*, i8** %inner1, align 8, !dbg !32, !tbaa !34
  %tobool = icmp eq i8* %0, null, !dbg !41
  br i1 %tobool, label %if.end, label %if.then, !dbg !42

if.then:                                          ; preds = %entry
  %inner2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 1, i32 1, !dbg !43
  %1 = load %struct.MyStruct*, %struct.MyStruct** %inner2, align 8, !dbg !43, !tbaa !45
  %firstField = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %1, i64 0, i32 0, !dbg !46
  %2 = load i32, i32* %firstField, align 8, !dbg !46, !tbaa !47
  call void @llvm.dbg.value(metadata i32 %2, metadata !27, metadata !DIExpression()), !dbg !31
  br label %if.end, !dbg !48

if.end:                                           ; preds = %if.then, %entry
  %secret_int.0 = phi i32 [ %2, %if.then ], [ 0, %entry ], !dbg !49
  call void @llvm.dbg.value(metadata i32 %secret_int.0, metadata !27, metadata !DIExpression()), !dbg !31
  call void @llvm.dbg.value(metadata i32 0, metadata !28, metadata !DIExpression()), !dbg !50
  %cmp9 = icmp sgt i32 %n, 0, !dbg !51
  br i1 %cmp9, label %for.body.lr.ph, label %for.cond.cleanup, !dbg !53

for.body.lr.ph:                                   ; preds = %if.end
  %idxprom12 = zext i32 %n to i64, !dbg !54
  %arrayidx = getelementptr inbounds i32, i32* %arr, i64 %idxprom12, !dbg !54
  %3 = load i32, i32* %arrayidx, align 4, !dbg !54, !tbaa !55
  %4 = mul i32 %3, %n, !dbg !53
  call void @llvm.dbg.value(metadata i32 undef, metadata !28, metadata !DIExpression()), !dbg !50
  call void @llvm.dbg.value(metadata i32 undef, metadata !27, metadata !DIExpression()), !dbg !31
  call void @llvm.dbg.value(metadata i32 undef, metadata !28, metadata !DIExpression(DW_OP_plus_uconst, 1, DW_OP_stack_value)), !dbg !50
  %5 = add i32 %secret_int.0, %4, !dbg !53
  br label %for.cond.cleanup, !dbg !56

for.cond.cleanup:                                 ; preds = %for.body.lr.ph, %if.end
  %secret_int.1.lcssa = phi i32 [ %secret_int.0, %if.end ], [ %5, %for.body.lr.ph ], !dbg !31
  call void @llvm.dbg.value(metadata i32 %secret_int.1.lcssa, metadata !27, metadata !DIExpression()), !dbg !31
  %firstField2 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %s, i64 0, i32 0, !dbg !56
  %6 = load i32, i32* %firstField2, align 8, !dbg !56, !tbaa !47
  %add3 = add nsw i32 %6, %secret_int.1.lcssa, !dbg !57
  ret i32 %add3, !dbg !58
}

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

attributes #0 = { norecurse nounwind readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable willreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 10.0.0 (git@github.com:llvm/llvm-project.git a2f6ae9abffcba260c22bb235879f0576bf3b783)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, nameTableKind: None)
!1 = !DIFile(filename: "../test.c", directory: "/home/dat14hol/git/llvm-project/build")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 10.0.0 (git@github.com:llvm/llvm-project.git a2f6ae9abffcba260c22bb235879f0576bf3b783)"}
!7 = distinct !DISubprogram(name: "my_fun", scope: !1, file: !1, line: 13, type: !8, scopeLine: 13, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !23)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !11, !22, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !DIDerivedType(tag: DW_TAG_typedef, name: "MyStruct", file: !1, line: 11, baseType: !12)
!12 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "MyStruct", file: !1, line: 6, size: 192, elements: !13)
!13 = !{!14, !15}
!14 = !DIDerivedType(tag: DW_TAG_member, name: "firstField", scope: !12, file: !1, line: 7, baseType: !10, size: 32)
!15 = !DIDerivedType(tag: DW_TAG_member, name: "secondField", scope: !12, file: !1, line: 8, baseType: !16, size: 128, offset: 64)
!16 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Inner", file: !1, line: 2, size: 128, elements: !17)
!17 = !{!18, !20}
!18 = !DIDerivedType(tag: DW_TAG_member, name: "inner1", scope: !16, file: !1, line: 3, baseType: !19, size: 64)
!19 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!20 = !DIDerivedType(tag: DW_TAG_member, name: "inner2", scope: !16, file: !1, line: 4, baseType: !21, size: 64, offset: 64)
!21 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !12, size: 64)
!22 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !10, size: 64)
!23 = !{!24, !25, !26, !27, !28}
!24 = !DILocalVariable(name: "s", arg: 1, scope: !7, file: !1, line: 13, type: !11)
!25 = !DILocalVariable(name: "arr", arg: 2, scope: !7, file: !1, line: 13, type: !22)
!26 = !DILocalVariable(name: "n", arg: 3, scope: !7, file: !1, line: 13, type: !10)
!27 = !DILocalVariable(name: "secret_int", scope: !7, file: !1, line: 14, type: !10)
!28 = !DILocalVariable(name: "i", scope: !29, file: !1, line: 20, type: !10)
!29 = distinct !DILexicalBlock(scope: !7, file: !1, line: 20, column: 3)
!30 = !DILocation(line: 13, column: 21, scope: !7)
!31 = !DILocation(line: 0, scope: !7)
!32 = !DILocation(line: 15, column: 19, scope: !33)
!33 = distinct !DILexicalBlock(scope: !7, file: !1, line: 15, column: 5)
!34 = !{!35, !40, i64 8}
!35 = !{!"MyStruct", !36, i64 0, !39, i64 8}
!36 = !{!"int", !37, i64 0}
!37 = !{!"omnipotent char", !38, i64 0}
!38 = !{!"Simple C/C++ TBAA"}
!39 = !{!"Inner", !40, i64 0, !40, i64 8}
!40 = !{!"any pointer", !37, i64 0}
!41 = !DILocation(line: 15, column: 5, scope: !33)
!42 = !DILocation(line: 15, column: 5, scope: !7)
!43 = !DILocation(line: 16, column: 30, scope: !44)
!44 = distinct !DILexicalBlock(scope: !33, file: !1, line: 15, column: 27)
!45 = !{!35, !40, i64 16}
!46 = !DILocation(line: 16, column: 38, scope: !44)
!47 = !{!35, !36, i64 0}
!48 = !DILocation(line: 17, column: 2, scope: !44)
!49 = !DILocation(line: 0, scope: !33)
!50 = !DILocation(line: 0, scope: !29)
!51 = !DILocation(line: 20, column: 21, scope: !52)
!52 = distinct !DILexicalBlock(scope: !29, file: !1, line: 20, column: 3)
!53 = !DILocation(line: 20, column: 3, scope: !29)
!54 = !DILocation(line: 0, scope: !52)
!55 = !{!36, !36, i64 0}
!56 = !DILocation(line: 22, column: 11, scope: !7)
!57 = !DILocation(line: 22, column: 22, scope: !7)
!58 = !DILocation(line: 22, column: 2, scope: !7)
