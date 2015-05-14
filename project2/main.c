//
//  main.c
//  project2
//
//  Created by Blurry on 2015/4/17.
//  Copyright (c) 2015年 Mac. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADD 0x20
#define SUB 0x22
#define AND 0x24
#define OR 0x25
#define XOR 0x26
#define NOR 0x27
#define NAND 0x28
#define SLT 0x2A
#define SLL 0x00
#define SRL 0x02
#define SRA 0x03
#define JR 0x08

#define ADDI 0x08
#define LW 0x23
#define LH 0x21
#define LHU 0x25
#define LB 0x20
#define LBU 0x24
#define SW 0x2b
#define SH 0x29
#define SB 0x28
#define LUI 0x0f
#define ANDI 0x0c
#define ORI 0x0d
#define NORI 0x0e
#define SLTI 0x0a
#define BEQ 0x04
#define BNE 0x05

#define J 0x02
#define JAL 0x03

static int * code_i, * code_d;
static int data_i[256] , data_d[256] ,reg[32] ,reg_buf[32] , PC ,PC_old;
static int cycle_count , HaltFlag ,flushflag = 0, stallflag = 0;

static int store_flag = 0,store_rt = 0, PCsaved = 0, fwdflag_rs = 0, fwdflag_rt = 0, fwd_rs = 0, fwd_rt = 0, EXfwd_rs = 0, EXfwd_rt = 0;

static struct FORWARD{
    char *where;
    int flag;
}fwd_EX_rs = {NULL, 0}, fwd_EX_rt = {NULL, 0}, init_fwd = {NULL, 0};

typedef struct INSTRUCTION{
    int IR;
    int type; // 1: R, 2: I, 3: J
    char *name;
    int op;
    int rd;
    int rs;
    int rt;
    int shamt;
    int funct;
    int imm;
    int address;
}INSTR;

static struct BUFFER_BETWEEN_PIPE{
    INSTR instr;
    int ALUout;
}init = {{0,0,"NOP",0,0,0,0,0,0,0,0},0},
IFIDin, IFIDout = {{0,0,"NOP"},0},
IDEXin, IDEXout = {{0,0,"NOP"},0},
EXMEMin, EXMEMout = {{0,0,"NOP"},0},
MEMWBin, MEMWBout = {{0,0,"NOP"},0},
WB = {{0,0,"NOP"},0};

static char *opcode_R[12] = {
    "ADD", "SUB", "AND", "OR", "XOR", "NOR", "NAND", "SLT",
    "SLL", "SRL", "SRA", "JR"
};
static int mapping_R[12] = {
    0x20, 0x22, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2A,
    0x00, 0x02, 0x03, 0x08
};
static char *opcode_I[16] = {
    "ADDI", "LW", "LH", "LHU", "LB", "LBU", "SW", "SH",
    "SB", "LUI", "ANDI", "ORI", "NORI", "SLTI", "BEQ", "BNE"
};
static int mapping_I[16] = {
    0x08, 0x23, 0x21, 0x25, 0x20, 0x24, 0x2B, 0x29,
    0x28, 0x0F, 0x0C, 0x0D, 0x0E, 0x0A, 0x04, 0x05
};
static char *opcode_J[2] = { "J", "JAL" };
static int mapping_J[2] = {
    0x02, 0x03
};

void ReadFile();
void LoadInMem();
int Change_endian(int c);
/*instruction fetch (IF), instruction decode (ID), ALU execution (EX), data
memory access (DM) and write back (WB).*/
void InstructionFetch();
void InstructionDecode();
void ALUexecution();
void DataMemoryAccess();
void WriteBack();
void Synchronize();
void PrintSnapshot(FILE* fw_ptr );
void ErrorDetect(int err);
void TestError(int in1 , int in2 , int out ,int type );

int main(void) {
    
    FILE * fw_ptr = fopen("snapshot.rpt", "w");
    FILE * fw_err = fopen("error_dump.rpt", "w");
    
    
    ReadFile();
    
    /* clean array first */
    int i;
    for (i = 0; i < 256 ; i++ ) {
        data_i[i] = 0;
        data_d[i] = 0;
    }
    for (i = 0; i < 32 ; i++ ) {
        reg[i] = 0;
        reg_buf[i] = 0;
    }
    PC = 0;
    PC_old = 0;
    cycle_count = 0;
    HaltFlag = 0;
    
    LoadInMem();
    
    while (1) {
        //printf("begin while.\n");
        
        EXfwd_rt = EXfwd_rs = fwd_rs = fwd_rt = fwdflag_rs = fwdflag_rt = PCsaved =
        stallflag = flushflag = reg[0] = 0;
        fwd_EX_rs = fwd_EX_rt = init_fwd;
        
        IFIDin = IDEXin = EXMEMin = MEMWBin = init;
        
        if (PC % 4 != 0 || PC > 1024 || PC < 0) {
            printf("PC is wrong in cycle %d\n",cycle_count);
            exit(1);
        }
        if (HaltFlag == 1) {
            exit(1);
        }
        WriteBack();
        DataMemoryAccess();
        ALUexecution();
        InstructionDecode();
        InstructionFetch();
        
        PrintSnapshot(fw_ptr );
        
        if(MEMWBout.instr.op == 0x3f && IDEXin.instr.op == 0x3f && EXMEMin.instr.op == 0x3f && MEMWBin.instr.op == 0x3f
           && ((IFIDin.instr.IR >> 26 ) & 0x3f == 0x3f)) break;
        
        Synchronize();
        
        if (stallflag == 1) PC = PC;
        else if(flushflag == 1){
            //printf("%d      ",PCsaved);
            PC = PCsaved;}
        else PC += 4;
        
        cycle_count ++ ;
        if (cycle_count > 500000) {
            printf("cycle counts more than 500,000 .\n");
            exit(1);
        }
        
        
    }
    fclose(fw_ptr);
    fclose(fw_err);
    
    
    return 0;
}
void InstructionFetch(){
    IFIDin.instr.IR = data_i[PC/4];
    PC_old = PC;
    //printf("PC = %d, in cycle %d\n",PC ,cycle_count);
}
void InstructionDecode(){
    IDEXin = IFIDout;
    if (IDEXin.instr.IR == 0) {
        IDEXin.instr.name = "NOP";
        return;
    }
   
    IDEXin.instr.op = (IDEXin.instr.IR >> 26) & 0x3f;
    if (IDEXin.instr.op == 0x3f) {IDEXin.instr.name = "HALT"; return;}
    
    /* distinguish type */
    if (IDEXin.instr.op == 0x00) IDEXin.instr.type = 1;  //R type
    else if (IDEXin.instr.op == 0x02 || IDEXin.instr.op == 0x03) IDEXin.instr.type = 3;  //J type
    else IDEXin.instr.type = 2;  //I type
    
    switch (IDEXin.instr.type) {
        case 1:
            
            IDEXin.instr.funct = IDEXin.instr.IR & 0x3f;
            if(!(IDEXin.instr.funct == SLL || IDEXin.instr.funct == SRL || IDEXin.instr.funct == SRA))
                IDEXin.instr.rs = (IDEXin.instr.IR >> 21) & 0x1f;
            else
                IDEXin.instr.shamt = (IDEXin.instr.IR >> 6) & 0x1f;
            if (IDEXin.instr.funct != JR){
                IDEXin.instr.rd = (IDEXin.instr.IR >> 11) & 0x1f;
                IDEXin.instr.rt = (IDEXin.instr.IR >> 16) & 0x1f;
            }
            if (IDEXin.instr.funct == SLL && IDEXin.instr.rt == 0 && IDEXin.instr.rd == 0 && IDEXin.instr.shamt == 0) {
                IDEXin.instr.name = "NOP";
                return;
            }
            
            int i ;
            for (i = 0; i < 12; i++) {
                if (IDEXin.instr.funct == mapping_R[i])
                    IDEXin.instr.name = opcode_R[i];
            }
            if (IDEXin.instr.name == NULL) {
                printf("ID state : R_type function is wrong , in Cycle %d .\n",cycle_count);
                exit(-1);
            }
            
            
            
            /* detect stall (only when load instr ~~~ ) */
            if (IDEXout.instr.rt != 0 && (IDEXin.instr.rs == IDEXout.instr.rt || IDEXin.instr.rt == IDEXout.instr.rt)
                && ( IDEXout.instr.op == LW || IDEXout.instr.op == LH || IDEXout.instr.op == LHU ||
                    IDEXout.instr.op == LB || IDEXout.instr.op == LBU )){
                stallflag = 1 ;
                return;
                }
            
            
            /* Conditional branches and unconditional branches are evaluated during the ID stage. 
                means branch 指令要在ID階段即得知 PC 位置 */
            
            if (IDEXin.instr.funct == JR) {
                /* detect stall */
                switch (IDEXout.instr.type) {
                    case 1:
                        if (IDEXout.instr.rd != 0 && IDEXin.instr.rs == IDEXout.instr.rd ){
                            stallflag = 1 ;
                            return;
                        }
                        break;
                    case 2:
                        if (IDEXout.instr.rt != 0 && (IDEXin.instr.rs == IDEXout.instr.rt )
                            && ( IDEXout.instr.op != SW && IDEXout.instr.op != SH && IDEXout.instr.op != SB
                                && IDEXout.instr.op != BEQ && IDEXout.instr.op != BNE )){
                            stallflag = 1 ;
                            return;
                        }
                        break;
                    default:
                        break;
                }
                
                /* detect forward */
                switch (EXMEMout.instr.type) {
                    case 1:
                        if (EXMEMout.instr.rd != 0 && (IDEXin.instr.rs == EXMEMout.instr.rd) ) {
                            fwdflag_rs = 1;
                            fwd_rs = EXMEMout.ALUout;
                        }
                        /*if (EXMEMout.instr.rd != 0 && (IDEXin.instr.rt == EXMEMout.instr.rd) ){
                            fwdflag_rt = 1;
                            fwd_rt = EXMEMout.ALUout;
                        }*/
                        break;
                    case 2:
                        if (( EXMEMout.instr.op == LW || EXMEMout.instr.op == LH || EXMEMout.instr.op == LHU ||
                              EXMEMout.instr.op == LB || EXMEMout.instr.op == LBU )) {
                            if (EXMEMout.instr.rt != 0 && (IDEXin.instr.rs == EXMEMout.instr.rt)) {
                                stallflag = 1;
                                return;
                            }
                            
                        }
                        else if(EXMEMout.instr.op != SW && EXMEMout.instr.op != SH && EXMEMout.instr.op != SB
                                && EXMEMout.instr.op != BEQ && EXMEMout.instr.op != BNE ){
                            if (EXMEMout.instr.rt != 0 && (IDEXin.instr.rs == EXMEMout.instr.rt) ) {
                                fwdflag_rs = 1;
                                fwd_rs = EXMEMout.ALUout;
                            }
                            /*if (EXMEMout.instr.rt != 0 && (IDEXin.instr.rt == EXMEMout.instr.rt) ){
                                fwdflag_rt = 1;
                                fwd_rt = EXMEMout.ALUout;
                            }*/
                        }
                        
                        break;
                    case 3:
                        if (EXMEMout.instr.op == JAL) {
                            if (IDEXin.instr.rs == 31) {
                                fwdflag_rs = 1;
                                fwd_rs = EXMEMout.ALUout;
                            }
                        }
                        break;
                    }
                
                if (fwdflag_rs == 1) PCsaved = fwd_rs;
                else PCsaved = reg[IDEXin.instr.rs];
                flushflag = 1;
                return;
                
                }
            break;
        case 2:
            
            IDEXin.instr.imm = ((IDEXin.instr.IR  & 0xffff) << 16 ) >> 16;
            IDEXin.instr.rt = (IDEXin.instr.IR >> 16) & 0x1f ;
            if(IDEXin.instr.op != LUI) IDEXin.instr.rs = (IDEXin.instr.IR >> 21) & 0x1f ;
            
            //int i ;
            for (i = 0; i < 16; i++) {
                if (IDEXin.instr.op == mapping_I[i])
                    IDEXin.instr.name = opcode_I[i];
            }
            if (IDEXin.instr.name == NULL) {
                printf("ID state : I_type function is wrong , in Cycle %d .\n",cycle_count);
                exit(-1);
            }
            
            /* detect stall (only when load instr ~~~ ) */
            if ( IDEXout.instr.op == LW || IDEXout.instr.op == LH || IDEXout.instr.op == LHU ||
                 IDEXout.instr.op == LB || IDEXout.instr.op == LBU ){
                if (IDEXout.instr.rt != 0 && (IDEXin.instr.rs == IDEXout.instr.rt ) ){
                    stallflag = 1;
                    return;
                }
                else if (IDEXin.instr.op == SW || IDEXin.instr.op == SH || IDEXin.instr.op == SB ){ /* store 兩個都會用到(rs + rt) */
                    if (IDEXout.instr.rt != 0 && (IDEXin.instr.rt == IDEXout.instr.rt )) {
                        stallflag = 1;
                        return;
                    }
                }
            }
            /* special detect for BEQ BNE branch */
            if (IDEXin.instr.op == BEQ || IDEXin.instr.op == BNE) {
                /* detect stall */
                switch (IDEXout.instr.type) {
                    case 1:
                        if (IDEXout.instr.rd != 0
                            && (IDEXin.instr.rs == IDEXout.instr.rd || IDEXin.instr.rt == IDEXout.instr.rd )){
                            stallflag = 1 ;
                            return;
                        }
                        break;
                    case 2:
                        if (IDEXout.instr.rt != 0 && (IDEXin.instr.rs == IDEXout.instr.rt || IDEXin.instr.rt == IDEXout.instr.rt )
                            && ( IDEXout.instr.op != SW && IDEXout.instr.op != SH && IDEXout.instr.op != SB
                                && IDEXout.instr.op != BEQ && IDEXout.instr.op != BNE )){
                                stallflag = 1 ;
                                return;
                            }
                        break;
                    default:
                        break;
                }
                
                /* detect forward */
                switch (EXMEMout.instr.type) {
                    case 1:
                        //printf("!!!!!in %d , out %d\n",EXMEMin.ALUout , EXMEMout.ALUout);
                        if (EXMEMout.instr.rd != 0 && (IDEXin.instr.rs == EXMEMout.instr.rd) ) {
                            fwdflag_rs = 1;
                            fwd_rs = EXMEMout.ALUout;
                        }
                        if (EXMEMout.instr.rd != 0 && (IDEXin.instr.rt == EXMEMout.instr.rd) ){
                            fwdflag_rt = 1;
                            fwd_rt = EXMEMout.ALUout;
                         }
                        break;
                    case 2:
                        
                        if (( EXMEMout.instr.op == LW || EXMEMout.instr.op == LH || EXMEMout.instr.op == LHU ||
                             EXMEMout.instr.op == LB || EXMEMout.instr.op == LBU )) {
                            //printf("hello!\n");
                            //printf("%d %d %d\n",EXMEMout.instr.rt,IDEXin.instr.rs,IDEXin.instr.rt);
                            if (EXMEMout.instr.rt != 0 && (IDEXin.instr.rs == EXMEMout.instr.rt || IDEXin.instr.rt == EXMEMout.instr.rt )) {
                                stallflag = 1;
                                return;
                            }
                        }
                        else if(EXMEMout.instr.op != SW && EXMEMout.instr.op != SH && EXMEMout.instr.op != SB
                                && EXMEMout.instr.op != BEQ && EXMEMout.instr.op != BNE ){
                            if (EXMEMout.instr.rt != 0 && (IDEXin.instr.rs == EXMEMout.instr.rt) ) {
                                fwdflag_rs = 1;
                                fwd_rs = EXMEMout.ALUout;
                            }
                            if (EXMEMout.instr.rt != 0 && (IDEXin.instr.rt == EXMEMout.instr.rt) ){
                                fwdflag_rt = 1;
                                fwd_rt = EXMEMout.ALUout;
                             }
                        }
                        
                        break;
                    case 3:
                        if (EXMEMout.instr.op == JAL) {
                            if (IDEXin.instr.rs == 31) {
                                fwdflag_rs = 1;
                                fwd_rs = EXMEMout.ALUout;
                            }
                            if (IDEXin.instr.rt == 31) {
                                fwdflag_rt = 1;
                                fwd_rt = EXMEMout.ALUout;
                            }
                        }
                        break;
                }
                if(IDEXin.instr.op == BEQ){
                    //printf("fwd_rs %d , fwd_rt %d ,in cycle %d\n",fwd_rs,fwd_rt,cycle_count);
                    if (fwdflag_rs == 0) fwd_rs = reg[IDEXin.instr.rs] ;
                    if (fwdflag_rt == 0) fwd_rt = reg[IDEXin.instr.rt] ;
                    if (fwd_rs == fwd_rt) {
                        PCsaved =  (PC )+ IDEXin.instr.imm*4 ; // PC counter
                        //printf("PC_old = %d, imm : %d _____\n", (PC_old) ,IDEXin.instr.imm);
                        //if((PC+4) != PCsaved)
                        flushflag = 1;
                        //printf("fwd_rs %d , fwd_rt %d ,in cycle %d\n",fwd_rs,fwd_rt,cycle_count);
                    }
                    
                }
                else if(IDEXin.instr.op == BNE){
                    if (fwdflag_rs == 0) fwd_rs = reg[IDEXin.instr.rs] ;
                    if (fwdflag_rt == 0) fwd_rt = reg[IDEXin.instr.rt] ;
                    if (fwd_rs != fwd_rt) {
                        PCsaved =   (PC ) + IDEXin.instr.imm*4 ; // PC counter
                        //if((PC+4) != PCsaved)
                        flushflag = 1;
                    }
                    
                }

            }
            
            break;
        case 3:
            IDEXin.instr.address = IDEXin.instr.IR & 0x3ffffff;
            for (i = 0; i < 2; i++) {
                if (IDEXin.instr.op == mapping_J[i])
                    IDEXin.instr.name = opcode_J[i];
            }
            if (IDEXin.instr.name == NULL) {
                printf("ID state : J_type function is wrong , in Cycle %d .\n",cycle_count);
                exit(-1);
            }
            if (IDEXin.instr.op == J) {
                 PCsaved = (  (PC)  & 0xf0000000 ) | (4 * IDEXin.instr.address) ;
            }
            else { // JAL
                PCsaved = (  (PC)  & 0xf0000000 ) | (4 * IDEXin.instr.address) ;
                IDEXin.ALUout =  (PC)  ; // PC counter
                IDEXin.instr.rs = 31; //因為要在WB才能存，先給定一個位子
            }
            //if((PC+4) != PCsaved)
                flushflag = 1;
            break;
        
        default:
            break;
    }
    
    
    
    
}
void ALUexecution(){
    
    EXMEMin = IDEXout ;
    if (EXMEMin.instr.funct == JR || EXMEMin.instr.op == JAL || EXMEMin.instr.op == J ||
            EXMEMin.instr.op == BEQ || EXMEMin.instr.op == BNE || EXMEMin.instr.IR == 0 ) {
        return;
    }
    
    //printf("ALU %s\n",EXMEMin.instr.name);
    switch (EXMEMin.instr.type) {
        case 1:
            
            
            /* detect forward */
            switch (MEMWBout.instr.type) {
                case 1:
                    if (MEMWBout.instr.rd != 0 && (EXMEMin.instr.rs == MEMWBout.instr.rd) ) {
                        fwd_EX_rs.where = "DM-WB";
                        fwd_EX_rs.flag = 1;
                        EXfwd_rs = MEMWBout.ALUout;
                    }
                    if (MEMWBout.instr.rd != 0 && (EXMEMin.instr.rt == MEMWBout.instr.rd) ){
                        fwd_EX_rt.where = "DM-WB";
                        fwd_EX_rt.flag = 1;
                        EXfwd_rt = MEMWBout.ALUout;
                    }
                    break;
                case 2:
                    
                    if (MEMWBout.instr.op != SW && MEMWBout.instr.op != SH && MEMWBout.instr.op != SB
                        && MEMWBout.instr.op != BEQ && MEMWBout.instr.op != BNE ) {
                        if (MEMWBout.instr.rt != 0 && (EXMEMin.instr.rs == MEMWBout.instr.rt) ) {
                            fwd_EX_rs.where = "DM-WB";
                            fwd_EX_rs.flag = 1;
                            EXfwd_rs = MEMWBout.ALUout;
                        }
                        if (MEMWBout.instr.rt != 0 && (EXMEMin.instr.rt == MEMWBout.instr.rt) ){
                            fwd_EX_rt.where = "DM-WB";
                            fwd_EX_rt.flag = 1;
                            EXfwd_rt = MEMWBout.ALUout;
                        }
                    }
                    
                    break;
                case 3:
                    if (MEMWBout.instr.op == JAL) {
                        if (EXMEMin.instr.rs == 31) {
                            fwd_EX_rs.where = "DM-WB";
                            fwd_EX_rs.flag = 1;
                            EXfwd_rs = MEMWBout.ALUout;
                        }
                        if (EXMEMin.instr.rt == 31) {
                            fwd_EX_rt.where = "DM-WB";
                            fwd_EX_rt.flag = 1;
                            EXfwd_rt = MEMWBout.ALUout;
                        }
                    }
                    break;
            }
            
            switch (EXMEMout.instr.type) {
                case 1:
                    if (EXMEMout.instr.rd != 0 && (EXMEMin.instr.rs == EXMEMout.instr.rd) ) {
                        fwd_EX_rs.where = "EX-DM";
                        fwd_EX_rs.flag = 1;
                        EXfwd_rs = EXMEMout.ALUout;
                    }
                    if (EXMEMout.instr.rd != 0 && (EXMEMin.instr.rt == EXMEMout.instr.rd) ){
                        fwd_EX_rt.where = "EX-DM";
                        fwd_EX_rt.flag = 1;
                        EXfwd_rt = EXMEMout.ALUout;
                    }
                    break;
                case 2:
                    if (( EXMEMout.instr.op == ADDI || EXMEMout.instr.op == LUI || EXMEMout.instr.op == ANDI ||
                         EXMEMout.instr.op == ORI || EXMEMout.instr.op == NORI || EXMEMout.instr.op == SLTI )) {
                        if (EXMEMout.instr.rt != 0 && (EXMEMin.instr.rs == EXMEMout.instr.rt) ) {
                            fwd_EX_rs.where = "EX-DM";
                            fwd_EX_rs.flag = 1;
                            EXfwd_rs = EXMEMout.ALUout;
                        }
                        if (EXMEMout.instr.rt != 0 && (EXMEMin.instr.rt == EXMEMout.instr.rt) ){
                            fwd_EX_rt.where = "EX-DM";
                            fwd_EX_rt.flag = 1;
                            EXfwd_rt = EXMEMout.ALUout;
                        }
                    }
                    
                    break;
                case 3:
                    if (EXMEMout.instr.op == JAL) {
                        if (EXMEMin.instr.rs == 31) {
                            fwd_EX_rs.where = "EX-DM";
                            fwd_EX_rs.flag = 1;
                            EXfwd_rs = EXMEMout.ALUout;
                        }
                        if (EXMEMin.instr.rt == 31) {
                            fwd_EX_rt.where = "EX-DM";
                            fwd_EX_rt.flag = 1;
                            EXfwd_rt = EXMEMout.ALUout;
                        }
                    }
                    break;
            }
            //printf("EX state : fwd_rs %d , fwd_rt %d ,in cycle %d\n",fwd_rs,fwd_rt,cycle_count);
            if (fwd_EX_rs.flag == 0) {
                EXfwd_rs = reg[EXMEMin.instr.rs];
            }
            if (fwd_EX_rt.flag == 0)
                EXfwd_rt = reg[EXMEMin.instr.rt];
            
            switch (EXMEMin.instr.funct) {
                case ADD:
                    TestError( EXfwd_rs ,EXfwd_rt , EXfwd_rs + EXfwd_rt,2);
                    EXMEMin.ALUout = EXfwd_rs + EXfwd_rt;
                    
                    break;
                case SUB:
                    TestError(EXfwd_rs,(-EXfwd_rt),EXfwd_rs + (-EXfwd_rt),2);
                    EXMEMin.ALUout = EXfwd_rs + (-EXfwd_rt);
                    
                    /*  value_rs =  reg[rs] ;
                     value_rt =  reg[rt] ;
                     reg[rd] = reg[rs] - reg[rt] ;
                     // TestError2
                     int s_in1 = (value_rs >> 31) & 1 ;
                     int s_in2 = (value_rt >> 31) & 1 ;
                     int s_out = (reg[rd] >> 31) & 1 ;
                     //N-P = P OR P-N = N
                     if ((s_in1 != s_in2 ) && ( s_in2 == s_out) ) {
                     ErrorDetect(2);
                     }*/
                    break;
                case AND:
                    EXMEMin.ALUout = EXfwd_rs & EXfwd_rt;
                    break;
                case OR:
                    EXMEMin.ALUout = EXfwd_rs | EXfwd_rt;
                    break;
                case XOR:
                    EXMEMin.ALUout = EXfwd_rs ^ EXfwd_rt;
                    break;
                case NOR:
                    EXMEMin.ALUout = ~(EXfwd_rs | EXfwd_rt);
                    break;
                case NAND:
                    EXMEMin.ALUout = ~(EXfwd_rs & EXfwd_rt);
                    break;
                case SLT:
                    EXMEMin.ALUout = (EXfwd_rs < EXfwd_rt)?1:0;
                    break;
                case SLL:
                    EXMEMin.ALUout = EXfwd_rt << EXMEMin.instr.shamt;
                    break;
                case SRL:/* unsigned shift */
                    if ( EXMEMin.instr.shamt == 0 )
                        EXMEMin.ALUout = EXfwd_rt;
                    else
                        EXMEMin.ALUout = (EXfwd_rt >> EXMEMin.instr.shamt) & ~(0xffffffff << (32 - EXMEMin.instr.shamt));
                    break;
                case SRA:/* signed shift */
                    EXMEMin.ALUout = EXfwd_rt >> EXMEMin.instr.shamt;
                    break;
                case JR:
                    //PC = reg[rs] ;
                    break;
                default:
                    break;
                }
            
            break;
        case 2:
            
            /* detect forward */
            switch (MEMWBout.instr.type) {
                case 1:
                    if (MEMWBout.instr.rd != 0 && (EXMEMin.instr.rs == MEMWBout.instr.rd) ) {
                        fwd_EX_rs.where = "DM-WB";
                        fwd_EX_rs.flag = 1;
                        EXfwd_rs = MEMWBout.ALUout;
                    }
                    if (EXMEMin.instr.op == SW || EXMEMin.instr.op == SH || EXMEMin.instr.op == SB) {
                        if (MEMWBout.instr.rd != 0 && (EXMEMin.instr.rt == MEMWBout.instr.rd) ){
                            fwd_EX_rt.where = "DM-WB";
                            fwd_EX_rt.flag = 1;
                            EXfwd_rt = MEMWBout.ALUout;
                        }
                    }
                    
                    break;
                case 2:
                    
                    if (MEMWBout.instr.op != SW && MEMWBout.instr.op != SH && MEMWBout.instr.op != SB
                        && MEMWBout.instr.op != BEQ && MEMWBout.instr.op != BNE ) {
                        if (MEMWBout.instr.rt != 0 && (EXMEMin.instr.rs == MEMWBout.instr.rt) ) {
                            fwd_EX_rs.where = "DM-WB";
                            fwd_EX_rs.flag = 1;
                            EXfwd_rs = MEMWBout.ALUout;
                        }
                        if (EXMEMin.instr.op == SW || EXMEMin.instr.op == SH || EXMEMin.instr.op == SB) {
                            if (MEMWBout.instr.rt != 0 && (EXMEMin.instr.rt == MEMWBout.instr.rt) ){
                                
                                fwd_EX_rt.where = "DM-WB";
                                fwd_EX_rt.flag = 1;
                                EXfwd_rt = MEMWBout.ALUout;
                                
                            }
                        }
                        
                    }
                    
                    break;
                case 3:
                    if (MEMWBout.instr.op == JAL) {
                        if (EXMEMin.instr.rs == 31) {
                            fwd_EX_rs.where = "DM-WB";
                            fwd_EX_rs.flag = 1;
                            EXfwd_rs = MEMWBout.ALUout;
                        }
                        if (EXMEMin.instr.op == SW || EXMEMin.instr.op == SH || EXMEMin.instr.op == SB) {
                            if (EXMEMin.instr.rt == 31) {
                                fwd_EX_rt.where = "DM-WB";
                                fwd_EX_rt.flag = 1;
                                EXfwd_rt = MEMWBout.ALUout;
                            }
                        }
                    }
                    break;
            }
            
            switch (EXMEMout.instr.type) {
                case 1:
                    if (EXMEMout.instr.rd != 0 && (EXMEMin.instr.rs == EXMEMout.instr.rd) ) {
                        fwd_EX_rs.where = "EX-DM";
                        fwd_EX_rs.flag = 1;
                        EXfwd_rs = EXMEMout.ALUout;
                    }
                    if (EXMEMin.instr.op == SW || EXMEMin.instr.op == SH || EXMEMin.instr.op == SB) {
                        if (EXMEMout.instr.rd != 0 && (EXMEMin.instr.rt == EXMEMout.instr.rd) ){
                            fwd_EX_rt.where = "EX-DM";
                            fwd_EX_rt.flag = 1;
                            EXfwd_rt = EXMEMout.ALUout;
                        }
                    }
                    break;
                case 2:
                        if (EXMEMout.instr.op == SW || EXMEMout.instr.op  == SH || EXMEMout.instr.op  == SB ||
                            EXMEMout.instr.op  == BEQ || EXMEMout.instr.op  == BNE ) {
                            break;
                        }
                        if (EXMEMout.instr.rt != 0 && (EXMEMin.instr.rs == EXMEMout.instr.rt) ) {
                            fwd_EX_rs.where = "EX-DM";
                            fwd_EX_rs.flag = 1;
                            EXfwd_rs = EXMEMout.ALUout;
                        }
                        if (EXMEMin.instr.op == SW || EXMEMin.instr.op == SH || EXMEMin.instr.op == SB) {
                            if (EXMEMout.instr.rt != 0 && (EXMEMin.instr.rt == EXMEMout.instr.rt) ){
                                fwd_EX_rt.where = "EX-DM";
                                fwd_EX_rt.flag = 1;
                                EXfwd_rt = EXMEMout.ALUout;
                            }
                        }
                    
                    
                    break;
                case 3:
                    if (EXMEMout.instr.op == JAL) {
                        if (EXMEMin.instr.rs == 31) {
                            fwd_EX_rs.where = "EX-DM";
                            fwd_EX_rs.flag = 1;
                            EXfwd_rs = EXMEMout.ALUout;
                        }
                        if (EXMEMin.instr.op == SW || EXMEMin.instr.op == SH || EXMEMin.instr.op == SB) {
                            if (EXMEMin.instr.rt == 31) {
                                fwd_EX_rt.where = "EX-DM";
                                fwd_EX_rt.flag = 1;
                                EXfwd_rt = EXMEMout.ALUout;
                            }
                        }
                    }
                    break;
            }
            
            if (fwd_EX_rs.flag == 0) {
                EXfwd_rs = reg[EXMEMin.instr.rs];
            }
            /*if (fwd_EX_rt.flag == 1){
                printf("%d %d, cycle %d\n",EXfwd_rt,reg[EXMEMin.instr.rt],cycle_count);            }*/
            
            
            //printf("?");
            int value_rs , value_imm;
            switch (EXMEMin.instr.op) {
                case ADDI :
                    value_rs =  EXfwd_rs ;
                    value_imm =  EXMEMin.instr.imm;
                    EXMEMin.ALUout = EXfwd_rs + EXMEMin.instr.imm;
                    //printf("%d %d \n",value_rs,value_imm);
                    TestError(value_rs, value_imm, EXMEMin.ALUout, 2);
                    break;
                case LW :
                    TestError(EXfwd_rs, EXMEMin.instr.imm , EXfwd_rs + EXMEMin.instr.imm ,2);
                    EXMEMin.ALUout = EXfwd_rs + EXMEMin.instr.imm;
                    break;
                case LH : /*load 整個word的一半 所以還是 /4 */
                    TestError(EXfwd_rs, EXMEMin.instr.imm , EXfwd_rs + EXMEMin.instr.imm ,2);
                    EXMEMin.ALUout = EXfwd_rs + EXMEMin.instr.imm;
                    break;
                case LHU :
                    TestError(EXfwd_rs, EXMEMin.instr.imm , EXfwd_rs + EXMEMin.instr.imm ,2);
                    EXMEMin.ALUout = EXfwd_rs + EXMEMin.instr.imm;
                    break;
                case LB :
                    TestError(EXfwd_rs, EXMEMin.instr.imm , EXfwd_rs + EXMEMin.instr.imm ,2);
                    EXMEMin.ALUout = EXfwd_rs + EXMEMin.instr.imm;
                    break;
                case LBU :
                    TestError(EXfwd_rs, EXMEMin.instr.imm , EXfwd_rs + EXMEMin.instr.imm ,2);
                    EXMEMin.ALUout = EXfwd_rs + EXMEMin.instr.imm;
                    break;
                case SW :
                    TestError(EXfwd_rs, EXMEMin.instr.imm , EXfwd_rs + EXMEMin.instr.imm ,2);
                    EXMEMin.ALUout = EXfwd_rs + EXMEMin.instr.imm;
                    break;
                case SH :
                    TestError(EXfwd_rs, EXMEMin.instr.imm , EXfwd_rs + EXMEMin.instr.imm ,2);
                    EXMEMin.ALUout = EXfwd_rs + EXMEMin.instr.imm;
                    break;
                case SB :
                    TestError(EXfwd_rs, EXMEMin.instr.imm , EXfwd_rs + EXMEMin.instr.imm ,2);
                    EXMEMin.ALUout = EXfwd_rs + EXMEMin.instr.imm;
                    break;
                case LUI :
                    EXMEMin.ALUout = EXMEMin.instr.imm << 16;
                    break;
                case ANDI :
                    EXMEMin.ALUout = EXfwd_rs & ( EXMEMin.instr.imm & 0x0000ffff );
                    break;
                case ORI :
                    EXMEMin.ALUout = EXfwd_rs | ( EXMEMin.instr.imm & 0x0000ffff) ;
                    break;
                case NORI :
                    EXMEMin.ALUout = ~(EXfwd_rs | (EXMEMin.instr.imm & 0x0000ffff));
                    break;
                case SLTI :
                    EXMEMin.ALUout = (EXfwd_rs < EXMEMin.instr.imm)? 1 : 0 ;
                    break;
                    
                default:
                    break;

            }
            break;
            
            
        case 3:// execute in ID state
            break;
        default:
            break;
    }
    
    
}
void DataMemoryAccess(){
    MEMWBin = EXMEMout;
    if (MEMWBin.instr.op != LW && MEMWBin.instr.op != LH && MEMWBin.instr.op != LHU &&
        MEMWBin.instr.op != LB && MEMWBin.instr.op != LBU && MEMWBin.instr.op != SW &&
        MEMWBin.instr.op != SH && MEMWBin.instr.op != SB ) {
        return;
    }
   // printf("MEM %d ,in cycle %d\n",MEMWBin.ALUout , cycle_count);
    switch (MEMWBin.instr.op) {
        case LW:
            if ( MEMWBin.ALUout < 0 || MEMWBin.ALUout > 1023) ErrorDetect(3);
            else if( MEMWBin.ALUout > 1020 )ErrorDetect(3);
            
            if (MEMWBin.ALUout % 4 != 0)
                ErrorDetect(4);
            if (HaltFlag==1)
                break;
            
            MEMWBin.ALUout = data_d[ MEMWBin.ALUout/ 4 ];
            break;
        case LH:
            if ( MEMWBin.ALUout < 0 || MEMWBin.ALUout > 1023) ErrorDetect(3);
            else if( MEMWBin.ALUout > 1022 )ErrorDetect(3);
            
            if (MEMWBin.ALUout % 2 != 0)
                ErrorDetect(4);
            if (HaltFlag==1)
                break;
            
            if (MEMWBin.ALUout % 4 == 0 )
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout / 4 ]  >> 16;
            else
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout / 4 ] << 16 >> 16 ;
            break;
        case LHU:
            if ( MEMWBin.ALUout < 0 || MEMWBin.ALUout > 1023) ErrorDetect(3);
            else if( MEMWBin.ALUout > 1022 )ErrorDetect(3);
            
            if (MEMWBin.ALUout % 2 != 0)
                ErrorDetect(4);
            if (HaltFlag==1)
                break;
            
            if (MEMWBin.ALUout % 4 == 0 )
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout / 4 ]  >> 16 & 0x0000ffff;
            else
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout / 4 ] << 16 >> 16 & 0x0000ffff;
            break;
        case LB:
            
            if ( MEMWBin.ALUout < 0 || MEMWBin.ALUout > 1023) ErrorDetect(3);
            
            if (HaltFlag==1)
                break;
            
            if (MEMWBin.ALUout %4 == 0)
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout / 4 ]  >> 24;
            else if (MEMWBin.ALUout %4 == 1)
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout/ 4 ] << 8 >> 24;
            else if (MEMWBin.ALUout %4 == 2)
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout / 4 ] << 16 >> 24;
            else
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout / 4 ] << 24 >> 24 ;
            
            break;
        case LBU:
            
            if ( MEMWBin.ALUout < 0 || MEMWBin.ALUout > 1023) ErrorDetect(3);
            
            if (HaltFlag==1)
                break;
            
            if (MEMWBin.ALUout %4 == 0)
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout / 4 ]  >> 24 & 0x000000ff;
            else if (MEMWBin.ALUout %4 == 1)
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout/ 4 ] << 8 >> 24 & 0x000000ff;
            else if (MEMWBin.ALUout %4 == 2)
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout / 4 ] << 16 >> 24 & 0x000000ff;
            else
                MEMWBin.ALUout = data_d[ MEMWBin.ALUout / 4 ] << 24 >> 24 & 0x000000ff;
            
            break;
        case SW:
            if ( MEMWBin.ALUout < 0 || MEMWBin.ALUout > 1023) ErrorDetect(3);
            else if( MEMWBin.ALUout > 1020 )ErrorDetect(3);
            
            if (MEMWBin.ALUout % 4 != 0)
                ErrorDetect(4);
            if  (HaltFlag==1)
                break;
            
            data_d[ MEMWBin.ALUout/ 4 ] = reg[MEMWBin.instr.rt];
            break;
        case SH:
            if ( MEMWBin.ALUout < 0 || MEMWBin.ALUout > 1023) ErrorDetect(3);
            else if( MEMWBin.ALUout > 1022 )ErrorDetect(3);
            
            if (MEMWBin.ALUout % 2 != 0)
                ErrorDetect(4);
            if (HaltFlag==1)
                break;
            if (MEMWBin.ALUout % 4 == 0 ){
                data_d[ MEMWBin.ALUout / 4 ] &= 0x0000ffff ;
                data_d[ MEMWBin.ALUout / 4 ] |= ((reg[ MEMWBin.instr.rt ] & 0x0000ffff)<< 16 );
            }
            else{
                data_d[ MEMWBin.ALUout / 4 ] &= 0xffff0000 ;
                data_d[ MEMWBin.ALUout / 4 ] |= (reg[MEMWBin.instr.rt] & 0x0000ffff);
            }
            break;
        case SB:
            
            if ( MEMWBin.ALUout < 0 || MEMWBin.ALUout > 1023) ErrorDetect(3);
            
            if (HaltFlag==1)
                break;
            
            if (MEMWBin.ALUout %4 == 0){
                data_d[ MEMWBin.ALUout / 4 ] &= 0x00ffffff ;
                data_d[ MEMWBin.ALUout / 4 ] |= ( (reg[ MEMWBin.instr.rt ] & 0x000000ff) << 24 );
            }
            else if (MEMWBin.ALUout %4 == 1){
                data_d[ MEMWBin.ALUout / 4 ] &= 0xff00ffff ;
                data_d[ MEMWBin.ALUout / 4 ] |= ( (reg[ MEMWBin.instr.rt ] & 0x000000ff) << 16 );
            }
            else if (MEMWBin.ALUout %4 == 2){
                data_d[ MEMWBin.ALUout / 4 ] &= 0xffff00ff ;
                data_d[ MEMWBin.ALUout / 4 ] |= ( (reg[ MEMWBin.instr.rt ] & 0x000000ff)  << 8 );
            }
            else{
                data_d[ MEMWBin.ALUout / 4 ] &= 0xffffff00 ;
                data_d[ MEMWBin.ALUout / 4 ] |=  (reg[ MEMWBin.instr.rt ] & 0x000000ff)  ;
            }
            break;
        default:
            break;
           // printf("MEM state : ALUout %d",MEMWBin.ALUout);
    }
    
}
void WriteBack(){
    WB = MEMWBout;
    //printf("WB : name = %s , in cycle %d \n", WB.instr.name ,cycle_count);
    //printf(" output %d",WB.ALUout);
    switch (WB.instr.type) {
        case 1:
            if(WB.instr.funct != JR){
                if (WB.instr.rd == 0) {
                    if (WB.instr.funct == SLL && WB.instr.rt == 0 && WB.instr.shamt == 0) {
                        ErrorDetect(0);
                    }
                    else ErrorDetect(1);
                }
                else{
                    reg[WB.instr.rd] = WB.ALUout;
                }
            
            }
            
            break;
        case 2:
            if (WB.instr.op == SW || WB.instr.op == SH || WB.instr.op == SB ||
                WB.instr.op == BEQ || WB.instr.op == BNE ) {
                return;
            }
            else {
                if (WB.instr.rt == 0) {
                    ErrorDetect(1);
                }
                else
                    reg[WB.instr.rt] = WB.ALUout;
            }
            break;
        case 3:
            if (WB.instr.op == JAL) {
                reg[31] = WB.ALUout;
            }
            break;
        default:
            break;
    }


}
void Synchronize(){
    if (flushflag == 1) {
        IFIDin = init;
    }
    if(stallflag == 1){
        IDEXin = init;
    }
    else IFIDout = IFIDin;
    
    IDEXout =  IDEXin;
    EXMEMout = EXMEMin;
    MEMWBout = MEMWBin;
    
    IFIDin = IDEXin = EXMEMin = MEMWBin = init;
    int i ;
    for (i = 0 ; i < 32 ; i++) reg_buf[i] =  reg[i];
    
    

}
void PrintSnapshot(FILE* fw_ptr ){
    //FILE * fw_ptr = fopen("snapshot.rpt", "a");
    
    int i ;
    fprintf( fw_ptr , "cycle %d\n" , cycle_count);
    for (i = 0; i < 32; i ++) {
        fprintf( fw_ptr , "$%02d: 0x%08X\n", i , reg_buf[i]);
    }
    fprintf( fw_ptr , "PC: 0x%08X\n" , PC );
    
    fprintf( fw_ptr , "IF: 0x%08X" , IFIDin.instr.IR);
        if(flushflag == 1 ) {
            /*if(IFIDin.instr.IR == 0 || ( ( (IFIDin.instr.IR >> 26) ==0 ) &&  ( (IFIDin.instr.IR << 11) ==0 ) ))fprintf( fw_ptr , "\n" );
            else */
            fprintf( fw_ptr , " to_be_flushed\n" );
            
        }
        else if(stallflag == 1) fprintf( fw_ptr , " to_be_stalled\n" );
        else fprintf( fw_ptr , "\n" );
    fprintf( fw_ptr , "ID: %s" , IDEXin.instr.name);
        if(stallflag == 1) fprintf( fw_ptr , " to_be_stalled" );
        else {
            if(fwdflag_rs == 1) fprintf( fw_ptr , " fwd_EX-DM_rs_$%d" , IDEXin.instr.rs);
            if(fwdflag_rt == 1) fprintf( fw_ptr , " fwd_EX-DM_rt_$%d" , IDEXin.instr.rt);
        }
        fprintf( fw_ptr , "\n" );
    fprintf( fw_ptr , "EX: %s" , EXMEMin.instr.name);
        if(fwd_EX_rs.flag == 1) fprintf( fw_ptr , " fwd_%s_rs_$%d" , fwd_EX_rs.where ,EXMEMin.instr.rs);
        if(fwd_EX_rt.flag == 1) fprintf( fw_ptr , " fwd_%s_rt_$%d" , fwd_EX_rt.where ,EXMEMin.instr.rt);
    
    fprintf( fw_ptr , "\n" );
    
    fprintf( fw_ptr , "DM: %s\n" , MEMWBin.instr.name);
    fprintf( fw_ptr , "WB: %s\n\n\n" , WB.instr.name);

}

void ReadFile(){ //put all binary code into men and detect error
    
    FILE * fr_i = fopen("iimage.bin", "rb");
    FILE * fr_d = fopen("dimage.bin", "rb");
    
    
    if(fr_i == NULL || fr_d == NULL){
        printf("Your testcase is WRONG or can't find.\n");
        exit(2);
    }
    
    int a = fseek(fr_i, 0L , SEEK_END);
    long int filesize_i = ftell(fr_i);
    int b = fseek(fr_i, 0L, SEEK_SET);
    //  filesize_i /= 4;
    
    if(a == 0 && b == 0)  code_i= malloc(filesize_i);
    else {
        printf("fseek iimage wrong and malloc failed .\n");
        exit(3);
    }
    
    a = fseek(fr_d, 0L , SEEK_END);
    long int filesize_d = ftell(fr_d);
    b = fseek(fr_d, 0L, SEEK_SET);
    //  filesize_d /= 4;
    
    if(a == 0 && b == 0)  code_d = malloc(filesize_d);
    else {
        printf("fseek dimage wrong and malloc failed .\n");
        exit(3);
    }
    
    fread(code_i, filesize_i , 1 , fr_i);
    fread(code_d, filesize_d , 1 , fr_d);
    //printf("%ld %d\n",sizeof(code_i),filesize_i);
    //printf("filesize = %ld %ld\n",filesize_i,filesize_d);
    
    int x = 0x1234;
    char y = *(char*)&x;
    
    if(y != 0x12){ // little endian
        //printf("shift begin\n") ;
        int i ;
        
        code_i[1] = Change_endian(code_i[1]);
        //printf("1 = %08x\n",code_i[1]);
        
        code_i[0] = Change_endian(code_i[0]);
        //printf("0 = %08x\n",code_i[0]);
        
        for ( i = 2; i < code_i[1] + 2; i++){
            
            code_i[i] = Change_endian(code_i[i]);
            //printf("i = %d ,%08x\n",i,code_i[i]);
        }
        //printf("code_i[1] = %08x\n",code_i[1]);
        
        //printf("origainal 1 = %08x\n",code_d[1]);
        code_d[1] = Change_endian(code_d[1]);
        //printf("1 = %08x\n",code_d[1]);
        
        code_d[0] = Change_endian(code_d[0]);
        //printf("0 = %08x\n",code_d[0]);
        if (code_d[1] > 1024) {
            //printf("brokennnnnnn .\n");
            exit(1);
        }
        for ( i = 2; i < code_d[1] + 2 ; i++){
            code_d[i] = Change_endian(code_d[i]);
            //printf("i = %d ,%08x\n",i,code_d[i]);
        }
        //printf("code_d[1] = %08x\n",code_d[1]);
    }
    
    
    
    fclose(fr_i);
    fclose(fr_d);
}
int Change_endian(int c){
    
    return ((c << 24 ) & 0xff000000 ) | ((c << 8 ) & 0x00ff0000 ) |
    ((c >> 8 ) & 0x0000ff00 ) | ((c >> 24 ) & 0x000000ff) ;
    
}


void LoadInMem(){
    
    PC = code_i[0];
    reg_buf[29] = reg[29] = code_d[0];
    int instr_num_i = code_i[1];
    int instr_num_d = code_d[1];
    //printf("PC = %08X, num = %08X .\n",PC ,instr_num_i);
    if ( PC + instr_num_i > 1024) {
        printf("iimage instructions more than 1K . Memory overflow .\n");
        printf("PC = %08X, num = %08X .\n",PC ,instr_num_i);
        
        exit(1);
    }
    if (instr_num_d > 1024) {
        printf("dimage instructions more than 1K . Memory overflow .\n");
        printf("$s = %08X, num = %08X .\n",reg[29] ,instr_num_d);
        exit(1);
    }
    
    int i , j;
    for (i = PC/4 , j = 2 ; j < 2 + instr_num_i ; i++, j++ )
        data_i[i] = code_i[j];
    
    for (i = 0 , j = 2 ; j < 2 + instr_num_d ; i++, j++ )
        data_d[i] = code_d[j];
    
    //printf("before free\n");
    /*free(code_i);
     free(code_d);*/
    
}

void TestError(int in1 , int in2 , int out ,int type ){
    int s_in1, s_in2 , s_out;
    
    switch (type) {
        case 2:/* number overflow */
            s_in1 = (in1 >> 31) & 1 ;
            s_in2 = (in2 >> 31) & 1 ;
            s_out = (out >> 31) & 1 ;
            //  printf("singned = %d %d %d in cycle = %d\n",s_in1,s_in2,s_out,cycle_count);
            //  printf("num = %8x %8x %8x in cycle = %d\n",in1,in2,out,cycle_count);
            if ( (s_in1 == s_in2 ) && ( s_out != s_in1 ) ) {
                ErrorDetect(2);
                
            }
            
            break;
       /* case 3:/* D Memory Address overflow  --   lw, lh, lhu, lb,
                lbu, sw, sh, sb, */
          /*  if( (in1 + in2) < 0 || (in1 + in2 )> 1023){
                //if(error3_re == 1){break;}
                ErrorDetect(3);
                
            }
            else if( (op == SW || op == LW) && ((in1 + in2 ) > 1020) ) ErrorDetect(3);
            else if( (op == SH || op == LH || op == LHU) && ((in1 + in2 ) > 1022) ){
                ErrorDetect(3);
            }
            break;*/
        default:
            break;
    }
}

void ErrorDetect(int err){
    FILE * fw_err = fopen("error_dump.rpt", "a");
    switch (err) {
        case 1:
            fprintf( fw_err, "In cycle %d: Write $0 Error\n", cycle_count+1);
            break;
        case 2:
            fprintf( fw_err , "In cycle %d: Number Overflow\n", cycle_count+1);
            break;
        case 3:
            fprintf( fw_err , "In cycle %d: Address Overflow\n", cycle_count+1);
            HaltFlag = 1;
            break;
        case 4:
            fprintf( fw_err , "In cycle %d: Misalignment Error\n", cycle_count+1);
            HaltFlag = 1;
            break;
        default:
            break;
    }
    
    fclose(fw_err);
    
    /* if (stop == 1) {
     exit(1);
     } */
    
    
    
}


