#include <stdio.h>
#include <stdlib.h>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>         
#include <assimp/postprocess.h>   

/*
 * Here is an high-level overview of the MDLX file format
 * 
 *          BAR   
 * |-------------------|
 * |        0x04       |
 * | |---------------| |
 * | |               | |
 * | |     MDL_H     | |
 * | |   |-------|   | |
 * | |   |MDL_P_H|   | |
 * | |   |MDL_P_H|   | |
 * | |   | ...   |   | |
 * | |   |-------|   | |
 * | |     ?????     | | <- unused in KH2, used in KH1
 * | |     MDL_P     | |
 * | | |-----------| | |
 * | | |   BONES   | | |
 * | | | |-------| | | |
 * | | | |  BONE | | | |
 * | | | |  BONE | | | |
 * | | | |  ...  | | | |
 * | | | |-------| | | |
 * | | |    SUBP   | | |
 * | | | |-------| | | |
 * | | | | VIFPKT| | | |
 * | | | | VIFPKT| | | |
 * | | | |  ...  | | | |
 * | | | |-------| | | |
 * | | |    DMA    | | |
 * | | | |-------| | | |
 * | | | |DMA_VIF| | | |
 * | | | | MAT_I | | | |
 * | | | | MAT_I | | | |
 * | | | |  ...  | | | |
 * | | | |DMA_VIF| | | |
 * | | | |  ...  | | | |
 * | | | --------- | | |
 * | | |    MAT    | | |
 * | | | |-------| | | |
 * | | | | MAT_I | | | |
 * | | | | MAT_I | | | |
 * | | | |  ...  | | | |
 * | | | |-------| | | |
 * | | |-----------| | |
 * | |     MDL_P     | |
 * | | |-----------| | |
 * | | |   BONES   | | |
 * | | | |-------| | | |
 * | | | |  BONE | | | |
 * | | | |  BONE | | | |
 * | | | |  ...  | | | |
 * | | | |-------| | | |
 * | | |    SUBP   | | |
 * | | | |-------| | | |
 * | | | | VIFPKT| | | |
 * | | | | VIFPKT| | | |
 * | | | |  ...  | | | |
 * | | | |-------| | | |
 * | | |    DMA    | | |
 * | | | |-------| | | |
 * | | | |DMA_VIF| | | |
 * | | | | MAT_I | | | |
 * | | | | MAT_I | | | |
 * | | | |  ...  | | | |
 * | | | |DMA_VIF| | | |
 * | | | |  ...  | | | |
 * | | | --------- | | |
 * | | |    MAT    | | |
 * | | | |-------| | | |
 * | | | | MAT_I | | | |
 * | | | | MAT_I | | | |
 * | | | |  ...  | | | |
 * | | | |-------| | | |
 * | | |-----------| | |
   | |      ...      | |
 * | |---------------| |
 * |                   |
 * |-------------------|
 * |        0x07       |
 * |    |---------|    |
 * |    |  TIM_0  |    |
 * |    |  TIM_1  |    |
 * |    |  .....  |    |
 * |    |---------|    |
 * |                   |
 * |-------------------|
 * |       0x17        |
 * |-------------------|
 * 
 *
 * 0x04 is the model. It contains a model header, followed by a model part per
 * texture, each including their list of bones, subpart to render by the VU1,
 * DMA tags to refer to the subparts and matrices. For more informations refer
 * to obj2kh2v.
 *
 * 0x07 is the texture container: it contains several textures under the TIM2
 * format. For more information refer to the tool building it.
 *
 * 0x17 is the object definition: contains collision, which bone lock-on target
 * is on, etc */

struct mdl_header {
    unsigned int nmb;
    unsigned int res1;
    unsigned int res2;
    unsigned int next_mdl_header; 
    unsigned short bone_cnt;
    unsigned short unk1;
    unsigned int bone_off;
    unsigned int unk_off;
    unsigned short mdl_subpart_cnt; 
    unsigned short unk3; 
};

struct mdl_subpart_header {
    unsigned int unk1;
    unsigned int texture_idx;
    unsigned int unk2;
    unsigned int unk3;
    unsigned int DMA_off;
    unsigned int mat_off;
    unsigned int unk4;
    unsigned int unk5;
};

struct bone_entry {
    unsigned short idx;
    unsigned short res1;
    unsigned int parent;
    unsigned int res2;
    unsigned int unk1;
    float sca_x;
    float sca_y;
    float sca_z;
    float sca_w;
    float rot_x;
    float rot_y;
    float rot_z;
    float rot_w;
    float trans_x;
    float trans_y;
    float trans_z;
    float trans_w;
};

struct DMA {
    unsigned short vif_len;
    unsigned short res1;
    unsigned int vif_off;
    unsigned int vif_inst1;
    unsigned int vif_inst2;
};
                    

int main(int argc, char* argv[]){
	printf("dae2mdlx\n--- Early rev, don't blame me if it eats your cat\n\n");
	if(argc<3){printf("usage: dae2mdlx test.kh2v model.dae"); return -1;}

		FILE *mdl;
        char empty[] = {0x00};

		mdl=fopen("test.kh2m","wb");



        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile( argv[2],
                                             aiProcess_Triangulate            |
                                             aiProcess_JoinIdenticalVertices  |
                                             aiProcess_SortByPType);
        if( !scene)
        {
            printf("error loading model!: %s", importer.GetErrorString());
            return -1;
        }


        // write kh2 dma in-game header
		for (int i=0; i<0x90;i++){fwrite(empty , 1 , sizeof(empty) , mdl);}


    /*Assimp::Exporter exporter;
    const aiExportFormatDesc* format = exporter.GetExportFormatDescription(0);
    exporter.Export(scene, "obj", "test.obj", scene->mFlags);*/

        unsigned int mesh_nmb= scene->mNumMeshes;
        printf("Number of meshes: %d\n", mesh_nmb);
        for(int i=0; i<mesh_nmb;i++){
            
            int vifpkt=1;
            int vert_count=0;
            int face_count=0;
            int bone_count=0;
            const aiMesh& mesh = *scene->mMeshes[i];
            // for some reason those arrays aren't initialized as 0...?
            int vertices_drawn[mesh.mNumVertices];
            for(int z=0;z<mesh.mNumVertices;z++){vertices_drawn[z]=0;}
            int bones_drawn[mesh.mNumBones];
            for(int z=0;z<mesh.mNumBones;z++){bones_drawn[z]=0;}
            int faces_drawn[mesh.mNumFaces];
            for(int z=0;z<mesh.mNumFaces;z++){faces_drawn[z]=0;}

            FILE *pkt;
            
            // we are writing a custom interlaced, bone-supporting obj here,
            // don't assume everything is following the obj standard! 
                printf("Generating Model Part %d, packet %d\n", i, vifpkt);
                for(int y=0; y<mesh.mNumFaces; y++){
           
                    // should be enough chars for a lifetime
                    char *filename = (char*)malloc(PATH_MAX*sizeof(char));
                    sprintf(filename, "%s_mp%d_vif%d.obj", argv[2], i, vifpkt);
                    pkt=fopen(filename, "a");

                    // we make the biggest vif packet, possible, for this, here
                    // is the size that each type of entry takes:
                    //
                    // bones - 1/4 of a qwc + 4 qwc(DMA tags)
                    // vertices - 1 qwc
                    // Face drawing - 3 qwc, UV and flags are bundled with it
                    // we here take the worst case scenario to ensure the vif
                    // packet < the maximum size 
                    if((((ceil((bone_count+1)/4)+(4*(bone_count+1))) + (vert_count+1) + ((face_count+1)*3))+4)<255){
                        // we update faces
                        faces_drawn[face_count]=y;
                        face_count++;
                        // we update bones
                        int tmp_check=0;
                        for(int d=0; d<mesh.mNumBones;d++){
                            for(int e=0;e<mesh.mBones[d].mWeights;e++){
                                if(mesh.mBones[d]->mWeights[e].mVertexId == mesh.mFaces[y].mIndices[0]] ||
                                   mesh.mBones[d]->mWeights[e].mVertexId == mesh.mFaces[y].mIndices[1]] ||
                                   mesh.mBones[d]->mWeights[e].mVertexId == mesh.mFaces[y].mIndices[2]]){
                                   for(int f=0; f<bone_count;f++){ if(bones_drawn[f]==mesh.mBones[d]->mWeights[e].mVertexId){tmp_check=1;} }
                                   // if we find a vertex of this face
                                   // associated to any bone and it is not a
                                   // duplicate we add it
                                   if(tmp_check=0){bones_drawn[bone_count]=mesh.mBones[d]->mWeights[e].mVertexId; bone_count++;}
                                }
                        }
                        tmp_check=0;
                        // we update vertices
                        for(int d=0; d<vert_count;d++){
                            if(vertices_drawn[d]==mesh.mVertices[mesh.mFaces[y].mIndices[0]].mVertexId){vertex_check=1;}
                        }
                        if(vertex_check==0){vertices_drawn[vert_count]=mesh.mVertices[mesh.mFaces[y].mIndices[0]].mVertexId; vert_count++;}
                        tmp_check=0;
                        for(int d=0; d<vert_count;d++){
                            if(vertices_drawn[d]==mesh.mVertices[mesh.mFaces[y].mIndices[1]].mVertexId){tmp_check=1;}
                        }
                        if(tmp_check==0){vertices_drawn[vert_count]=mesh.mVertices[mesh.mFaces[y].mIndices[1]].mVertexUd; vert_count++;}
                        tmp_check=0;
                        for(int d=0; d<vert_count;d++){
                            if(vertices_drawn[d]==mesh.mVertices[mesh.mFaces[y].mIndices[2]].mVertexId){tmp_check=1;}
                        }
                        if(tmp_check==0){vertices_drawn[vert_count]=mesh.mVertices[mesh.mFaces[y].mIndices[2]].mVertexId; vert_count++;}

                  }
                  else{
                    // if we are over the maximum size allowed for a packet we
                    // sort vertices per bones, rearrange the model to draw
                    // faces correctly and write this packet 
                    // TODO: sort vertices per bone, update faces index, write
                    // to file
                       // we do not sort bones as we sort vertices based on bone
                       // order
                       for(int i=0; i<bone_count; i++){ fprintf(pkt, "vb %d\n", bones_drawn[i]); } 
                      
                       //TODO: write here Mati and DMA
                        y--; vifpkt++; face_count=0;printf("Generating Model Part %d, packet %d\n", i, vifpkt);}
                  fclose(pkt);

            }
                printf("Generated Model Part %d, splitted in %d packets\n", i, vifpkt);
                for(int s=1;s<vifpkt+1;s++){
                    char *filename = (char*)malloc(PATH_MAX*sizeof(char));
                    char *makepkt = (char*)malloc((PATH_MAX+10)*sizeof(char));
                    char *kh2vname = (char*)malloc(PATH_MAX*sizeof(char));
                    sprintf(filename, "%s_mp%d_vif%d.obj", argv[2], i, s);
                    sprintf(kh2vname, "%s_mp%d_vif%d.kh2v", argv[2], i, s);
                    sprintf(makepkt, "obj2kh2v \"%s\"", filename);
                    system(makepkt); 

                    // we need to generate vif packets and create the file here
                    // but as it is filling up my hard drive I'm just removing
                    // the files for now
                    remove(filename);
                    remove(kh2vname);

                }
        }

        // we do not have a dae parser yet so we take vif packets directly
        FILE * dummy_vif = fopen(argv[1], "rb");

        fseek(dummy_vif, 0x24, SEEK_SET);
        char mat_vif_off;
        fread(&mat_vif_off, 4, 1, dummy_vif);
        fseek(dummy_vif, 0x0, SEEK_END);
        unsigned short vifpkt_len = ftell(dummy_vif)/16;
        fseek(dummy_vif, 0x0, SEEK_SET);

        int off_vif = ftell(mdl) - 0x90;

        size_t n, m;
        unsigned char buff[8192];
        do {
            n = fread(buff, 1, sizeof buff, dummy_vif);
            if (n) m = fwrite(buff, 1, n, mdl);
            else   m = 0;
        } while ((n > 0) && (n == m));
        

        int off_dma = ftell(mdl);
        // 910
        char end_dma[] = {0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00};
        unsigned short qwc_mat_len=4;
        unsigned short res_unk = 0x3000;
        unsigned int mat_idx = 0x0;
        unsigned int vif_off=0x0;
        fwrite(&vifpkt_len , 1 , sizeof(vifpkt_len) , mdl);
        fwrite(&res_unk , 1 , sizeof(res_unk) , mdl);
        fwrite(&off_vif , 1 , sizeof(off_vif) , mdl);
		for (int i=0; i<8;i++){fwrite(empty , 1 , sizeof(empty) , mdl);}
        fwrite(&qwc_mat_len , 1 , sizeof(qwc_mat_len) , mdl);
        fwrite(&res_unk , 1 , sizeof(res_unk) , mdl);
        fwrite(&mat_idx , 1 , sizeof(mat_idx) , mdl);
        unsigned char stcycl[] = {0x01, 0x01, 0x00, 0x01}; // stcycl 1,1
        fwrite(stcycl , 1 , sizeof(stcycl) , mdl);
        
        unsigned char unpack[] = {0x80, 0x04, 0x6c}; // unpack V4_32
        unsigned short unpack_end=0x6c04;
        fwrite(&mat_vif_off , 1 , sizeof(mat_vif_off) , mdl);
        fwrite(unpack , 1 , sizeof(unpack) , mdl);
        fwrite(end_dma , 1 , sizeof(end_dma) , mdl);

        

        int off_mat = ftell(mdl);
        unsigned int mat_dummy[] = { 0x01, 0x00, 0x0, 0x0 };
        fwrite(mat_dummy , 1 , sizeof(mat_dummy) , mdl);
        
        // fixing header offsets
        fseek(mdl, off_head, SEEK_SET);
        head->bone_off=off_bone - 0x90;
        fwrite(head , 1 , sizeof(struct mdl_header) , mdl);
        fseek(mdl, off_subh, SEEK_SET);
        subh->DMA_off=off_dma - 0x90;
        subh->mat_off=off_mat - 0x90;
        fwrite(subh , 1 , sizeof(struct mdl_subpart_header) , mdl);

		fclose(mdl);
        fclose(dummy_vif);
}


