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
    unsigned short unk2; 
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
    unsigned short parent;
    unsigned short res2;
    unsigned int unk1;
    unsigned int unk2;
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
};
                    

void write_packet(int vert_count, int bone_count, int face_count, int bones_drawn[], int faces_drawn[], int vertices_drawn[], int mp, int vifpkt, const aiMesh& mesh, char *name, int last){
 
                    // should be enough chars for a lifetime
                    char *filename = (char*)malloc(PATH_MAX*sizeof(char));
                    sprintf(filename, "%s_mp%d_pkt%d.obj", name, mp, vifpkt);
                    FILE *pkt=fopen(filename, "w");

                    /*
                    printf("%d, %d, %d\n", bone_count, vert_count, face_count);
                    for(int i=0; i<bone_count; i++){printf("%d, ", bones_drawn[i]);}
                    printf("\n");
                    for(int i=0; i<vert_count; i++){printf("%d, ", vertices_drawn[i]);}
                    printf("\n");
                    for(int i=0; i<face_count; i++){printf("%d, ", faces_drawn[i]);}
                    printf("\n");*/
                    // if we are over the maximum size allowed for a packet we
                    // sort vertices per bones, rearrange the model to draw
                    // to file
                       // we do not sort bones as we sort vertices based on bone
                       // order
                       int bone_to_vertex[bone_count];
                       for(int i=0; i<bone_count; i++){bone_to_vertex[i]=0;}
                       int vert_new_order[vert_count];
                       for(int i=0; i<vert_count; i++){vert_new_order[i]=0;}
                       int new_order_count=0;

                       // we check the number of vertices assigned to each bone
                       // in this packet, and reorganize them per bone
                       for(int d=0; d<bone_count;d++){
                            for(int e=0;e<mesh.mBones[bones_drawn[d]]->mNumWeights;e++){
                                for(int f=0;f<vert_count;f++){
                                    if(mesh.mBones[bones_drawn[d]]->mWeights[e].mVertexId == vertices_drawn[f]){
                                        bone_to_vertex[d]++;
                                    }
                                }
                            }
                       }

                       // we now sort vertices per bone order
                       for(int d=0; d<bone_count;d++){
                            for(int e=0;e<mesh.mBones[bones_drawn[d]]->mNumWeights;e++){
                                for(int f=0;f<vert_count;f++){
                                    int tmp_check=0;
                                    if(mesh.mBones[bones_drawn[d]]->mWeights[e].mVertexId == vertices_drawn[f]){ 
                                       for(int g=0; g<new_order_count;g++){ if(vert_new_order[g]==vertices_drawn[f]){tmp_check=1;} }
                                       if(tmp_check==0){vert_new_order[new_order_count]=vertices_drawn[f]; new_order_count++;}
                                }
                            }
                        }
                       }
                       /*
                    printf("Sorted vertices: \n");
                    for(int i=0; i<vert_count; i++){printf("%d, ", vert_new_order[i]);}
                    printf("\n");*/
                      
                       // we write the sorted model packet
                       for(int i=0; i<vert_count;i++){
                            fprintf(pkt, "v %f %f %f\n", mesh.mVertices[vert_new_order[i]].x,mesh.mVertices[vert_new_order[i]].y,mesh.mVertices[vert_new_order[i]].z );
                            // TODO: maybe check according to assimp doc min and
                            // max values for UV? Should be between 0 and 1
                            fprintf(pkt, "vt %f %f\n", mesh.mTextureCoords[0][vert_new_order[i]].x, mesh.mTextureCoords[0][vert_new_order[i]].y);
                       }
                       for(int i=0; i<bone_count; i++){ fprintf(pkt, "vb %d\n", bone_to_vertex[i]); } 
                       for(int i=0; i<face_count; i++){ 
                           int f1=-1;
                           int f2=-1;
                           int f3=-1;
                           int y=0;
                           while(f1==-1){
                               if(mesh.mFaces[faces_drawn[i]].mIndices[0] == vert_new_order[y]){f1=y;}
                               y++;
                           }
                           y=0;
                           while(f2==-1){
                               if(mesh.mFaces[faces_drawn[i]].mIndices[1] == vert_new_order[y]){f2=y;}
                               y++;
                           }
                           y=0;
                           while(f3==-1){
                               if(mesh.mFaces[faces_drawn[i]].mIndices[2] == vert_new_order[y]){f3=y;}
                               y++;
                           }
                           fprintf(pkt, "f %d %d %d\n", f1+1,f2+1, f3+1); 
                       } 

                    
                       
                    char *kh2vname = (char*)malloc(PATH_MAX*sizeof(char));
                    char *dmaname = (char*)malloc(PATH_MAX*sizeof(char));
                    char *matname = (char*)malloc(PATH_MAX*sizeof(char));
                    char *makepkt = (char*)malloc((PATH_MAX+10)*sizeof(char));
                    sprintf(kh2vname, "%s_mp%d_pkt%d.kh2v", name, mp, vifpkt);
                    sprintf(dmaname, "%s_mp%d_pkt%d.dma", name, mp, vifpkt);
                    sprintf(matname, "%s_mp%d_pkt%d.mat", name, mp, vifpkt);



                    fclose(pkt);

                    sprintf(makepkt, "obj2kh2v \"%s\"", filename);
                    system(makepkt); 

                    FILE *kh2v = fopen(kh2vname, "rb");
                    fseek(kh2v, 0x24, SEEK_SET);
                    char mat_vif_off;
                    fread(&mat_vif_off, 4, 1, kh2v);
                    fseek(kh2v, 0x18, SEEK_SET);
                    int mat_cnt;
                    fread(&mat_cnt, sizeof(int), 1, kh2v);


                    remove(filename);
                   
                     FILE *dma_file=fopen(dmaname, "wb");
                    struct DMA *dma_entry=(DMA*)malloc(sizeof(struct DMA));
                    fseek(kh2v, 0x0, SEEK_END);
                    dma_entry->vif_len = ftell(kh2v)/16;
                    dma_entry->res1 = 0x3000;
                    // TOFIX: we don't know yet where in the final file our packet will
                    // end up so we blank it out for now.
                    dma_entry->vif_off=0;
                    char vif_empty[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                    fwrite(dma_entry , 1 , sizeof(struct DMA) , dma_file);
                    fwrite(vif_empty , 1 , sizeof(vif_empty) , dma_file);
                    for(int i=0; i<bone_count; i++){
                        dma_entry->vif_len = 4;
                        dma_entry->res1 = 0x3000;
                        // we don't know yet where in the final file our packet will
                        // end up so we blank it out for now.
                        dma_entry->vif_off=bones_drawn[i];
                        unsigned char vif_inst[] = {0x01, 0x01, 0x00, 0x01, 0x00, 0x80, 0x04, 0x6C};
                        vif_inst[4]=mat_vif_off+(i*4);
                        fwrite(dma_entry , 1 , sizeof(struct DMA) , dma_file);
                        fwrite(vif_inst , 1 , sizeof(vif_inst) , dma_file);
                    }
                    char end_dma[] = {0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00};
                    fwrite(end_dma , 1 , sizeof(end_dma) , dma_file);

                    FILE *mat_file=fopen(matname, "wb");
                    fwrite(&mat_cnt, 1, sizeof(mat_cnt), mat_file); 
                    for(int i=0; i<bone_count; i++){ fwrite(&bones_drawn[i], 1, sizeof(bones_drawn[i]), mat_file); }
                    int end_mat=-1;
                    if(last){end_mat = 0;}
                    fwrite(&end_mat, 1, sizeof(end_mat), mat_file);
                    
                    fclose(dma_file);
                    fclose(mat_file);
                    // TODO: write Mati here
                    // remove(kh2vname);

}
int main(int argc, char* argv[]){
	printf("dae2mdlx\n--- Early rev, don't blame me if it eats your cat\n\n");
	if(argc<2){printf("usage: dae2mdlx model.dae\n"); return -1;}

		FILE *mdl;
        char empty[] = {0x00};

		mdl=fopen("test.kh2m","wb");



        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile( argv[1],
                                             aiProcess_Triangulate            |
                                             aiProcess_JoinIdenticalVertices  |
                                             aiProcess_SortByPType);
        if( !scene)
        {
            printf("error loading model!: %s", importer.GetErrorString());
            return -1;
        }


    

    /*Assimp::Exporter exporter;
    const aiExportFormatDesc* format = exporter.GetExportFormatDescription(0);
    exporter.Export(scene, "obj", "test.obj", scene->mFlags);*/

        unsigned int mesh_nmb= scene->mNumMeshes;
        int vifpkt[mesh_nmb];
        printf("Number of meshes: %d\n", mesh_nmb);
        for(int i=0; i<mesh_nmb;i++){
            
            vifpkt[i]=1;
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

            // we are writing a custom interlaced, bone-supporting obj here,
            // don't assume everything is following the obj standard! 
                // printf("Generating Model Part %d, packet %d\n", i+1, vifpkt);
                for(int y=0; y<mesh.mNumFaces; y++){
          
                    // we make the biggest vif packet, possible, for this, here
                    // is the size that each type of entry takes:
                    //
                    // header - 4 qwc
                    // bones - 1/4 of a qwc + 4 qwc(DMA tags)
                    // vertices - 1 qwc
                    // Face drawing - 3 qwc, UV and flags are bundled with it
                    // we here take the worst case scenario to ensure the vif
                    // packet < the maximum size 
                    if(((((ceil((bone_count+3)/4)+(4*(bone_count+3))) + (vert_count+3) + ((face_count+1)*3))+4)<255)){
                        // we update faces
                        faces_drawn[face_count]=y;
                        face_count++;
                        // we update bones
                        // printf("This face has the vertices %d %d %d\n",mesh.mFaces[y].mIndices[0],mesh.mFaces[y].mIndices[1], mesh.mFaces[y].mIndices[2]);
                        int tmp_check=0;
                        for(int d=0; d<mesh.mNumBones;d++){
                            for(int e=0;e<mesh.mBones[d]->mNumWeights;e++){
                                tmp_check=0;
                                if(mesh.mBones[d]->mWeights[e].mVertexId == mesh.mFaces[y].mIndices[0] ||
                                   mesh.mBones[d]->mWeights[e].mVertexId == mesh.mFaces[y].mIndices[1] ||
                                   mesh.mBones[d]->mWeights[e].mVertexId == mesh.mFaces[y].mIndices[2]){
                                   for(int f=0; f<bone_count;f++){ if(bones_drawn[f]==d){tmp_check=1;} }
                                   // if we find a vertex of this face
                                   // associated to any bone and it is not a
                                   // duplicate we add it
                                   if(tmp_check==0){bones_drawn[bone_count]=d; bone_count++;}
                                }
                            }
                        }
                        tmp_check=0;
                        // we update vertices
                        for(int d=0; d<vert_count;d++){
                            if(vertices_drawn[d]==mesh.mFaces[y].mIndices[0]){tmp_check=1;}
                        }
                        if(tmp_check==0){vertices_drawn[vert_count]=mesh.mFaces[y].mIndices[0]; vert_count++;}
                        tmp_check=0;
                        for(int d=0; d<vert_count;d++){
                            if(vertices_drawn[d]==mesh.mFaces[y].mIndices[1]){tmp_check=1;}
                        }
                        if(tmp_check==0){vertices_drawn[vert_count]=mesh.mFaces[y].mIndices[1]; vert_count++;}
                        tmp_check=0;
                        for(int d=0; d<vert_count;d++){
                            if(vertices_drawn[d]==mesh.mFaces[y].mIndices[2]){tmp_check=1;}
                        }
                        if(tmp_check==0){vertices_drawn[vert_count]=mesh.mFaces[y].mIndices[2]; vert_count++;}

                        if(y==mesh.mNumFaces-1){write_packet(vert_count, bone_count, face_count, bones_drawn, faces_drawn, vertices_drawn, i+1, vifpkt[i], mesh, argv[1], 1);}

                  }
                  else{
                      write_packet(vert_count, bone_count, face_count, bones_drawn, faces_drawn, vertices_drawn, i+1, vifpkt[i],  mesh, argv[1], 0);
                        y--; vifpkt[i]++; face_count=0; bone_count=0; vert_count=0;
                        for(int z=0;z<mesh.mNumVertices;z++){vertices_drawn[z]=0;}
                        for(int z=0;z<mesh.mNumBones;z++){bones_drawn[z]=0;}
                        for(int z=0;z<mesh.mNumFaces;z++){faces_drawn[z]=0;}
                        // printf("Generating Model Part %d, packet %d\n", i+1, vifpkt);
                        }
                  // fclose(pkt);

            }
                printf("Generated Model Part %d, splitted in %d packets\n", i+1, vifpkt[i]);
        }

        // now that we have all intermediate files we can finally begin creating
        // the actual model by assembling all of them together
        // write kh2 dma in-game header
		for (int i=0; i<0x90;i++){fwrite(empty , 1 , sizeof(empty) , mdl);}


        unsigned int old_mp=0;
        for(int i=0; i<mesh_nmb;i++){
            const aiMesh& mesh = *scene->mMeshes[i];
            if(old_mp){
                unsigned int cur_pos = ftell(mdl)-0x90;
                fseek(mdl, old_mp, SEEK_SET);
                fwrite(&cur_pos, 1, sizeof(cur_pos), mdl);
                fseek(mdl, cur_pos, SEEK_SET);
            }
            old_mp=ftell(mdl);
            struct mdl_header* head = (mdl_header*)malloc(sizeof(struct mdl_header));
            head->nmb=3;
            head->res1=0;
            head->res2=0;
            // this is where old_mp writes for the old packet
            head->next_mdl_header=0;
            head->bone_cnt=mesh.mNumBones;
            head->unk1=0;
            // we need to write this once we generated the bone tables
            head->bone_off=0;
            // as this table is unused nobody cares and we blank it out, saves
            // space
            head->unk_off=0;
            head->mdl_subpart_cnt=vifpkt[i];
            head->unk2=0;
            fwrite(head , 1 , sizeof(struct mdl_header) , mdl);

            for(int y=0; y<vifpkt[i]; y++){
                // write subheader here!
                struct mdl_subpart_header* subhead = (mdl_subpart_header*)malloc(sizeof(struct mdl_subpart_header));
                // TODO: verify what those unknowns are! 
                // we do not have any offset yet so we just blank out everything
                subhead->unk1=0;
                subhead->texture_idx=i;
                subhead->unk2=0;
                subhead->unk3=0;
                subhead->DMA_off=0;
                subhead->mat_off=0;
                subhead->unk4=0;
                subhead->unk5=0;
                fwrite(subhead , 1 , sizeof(struct mdl_subpart_header) , mdl);

            }

            // TOFIX: verify organization of all of those parts!!
            for(int y=0; y<mesh.mNumBones;y++){

                struct bone_entry* bone = (bone_entry*)malloc(sizeof(struct bone_entry));
                bone->idx=y;
                bone->res1=0;
                // FIXME: write correctly parent and coordinates absolutely!!!
                bone->parent=0;
                bone->res2=-1;
                bone->unk1=0;
                bone->sca_x=0;
                bone->sca_y=0;
                bone->sca_z=0;
                bone->sca_w=0;
                bone->rot_x=0;
                bone->rot_y=0;
                bone->rot_z=0;
                bone->rot_w=0;
                bone->trans_x=0;
                bone->trans_y=0;
                bone->trans_z=0;
                bone->trans_w=0;
                fwrite(bone , 1 , sizeof(struct bone_entry) , mdl);
            }
            
            for(int y=0; y<vifpkt[i]; y++){
                char *kh2vname = (char*)malloc(PATH_MAX*sizeof(char));
                sprintf(kh2vname, "%s_mp%d_pkt%d.kh2v", argv[1], i+1, y+1);
                FILE *vif_final = fopen(kh2vname, "rb");

                size_t n, m;
                unsigned char buff[8192];
                do {
                    n = fread(buff, 1, sizeof buff, vif_final);
                    if (n) m = fwrite(buff, 1, n, mdl);
                    else   m = 0;
                } while ((n > 0) && (n == m));

                fclose(vif_final);
                remove(kh2vname);
            }

            for(int y=0; y<vifpkt[i]; y++){
                char *dmaname = (char*)malloc(PATH_MAX*sizeof(char));
                sprintf(dmaname, "%s_mp%d_pkt%d.dma", argv[1], i+1, y+1);
                FILE *dma_final = fopen(dmaname, "rb");

                size_t n, m;
                unsigned char buff[8192];
                do {
                    n = fread(buff, 1, sizeof buff, dma_final);
                    if (n) m = fwrite(buff, 1, n, mdl);
                    else   m = 0;
                } while ((n > 0) && (n == m));

                fclose(dma_final);
                remove(dmaname);
            }

            for(int y=0; y<vifpkt[i]; y++){
                char *matname = (char*)malloc(PATH_MAX*sizeof(char));
                sprintf(matname, "%s_mp%d_pkt%d.mat", argv[1], i+1, y+1);
                FILE *mat_final = fopen(matname, "rb");

                size_t n, m;
                unsigned char buff[8192];
                do {
                    n = fread(buff, 1, sizeof buff, mat_final);
                    if (n) m = fwrite(buff, 1, n, mdl);
                    else   m = 0;
                } while ((n > 0) && (n == m));

                fclose(mat_final);
                remove(matname);
                while(ftell(mdl)%16!=0){fwrite(empty , 1 , sizeof(empty) , mdl);}

            }


            
        }


        /*
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
        fclose(dummy_vif);
        */
		fclose(mdl);
}

