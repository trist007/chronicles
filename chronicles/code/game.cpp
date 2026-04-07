#define PLAYER_MAX_SPEED 2.0f
#define PLAYER_ACCELERATION 10.0f
#define PLAYER_DECELERATION 5.0f

bool
Update(Player *p, float dt)
{
    p->isWalking = false;
    
    const bool *keys = SDL_GetKeyboardState(NULL);
    
    SDL_Event e;
    if (SDL_PollEvent(&e))
    {
        if (e.type == SDL_EVENT_QUIT)
        {
            return false;
        }
        if (e.type == SDL_EVENT_KEY_UP && e.key.key == SDLK_ESCAPE)
        {
            return false;
        }
    }
    
    if(p->yaw > PI32)
        p->yaw -= 2.0f * PI32;
    if(p->yaw < -PI32)
        p->yaw += 2.0f * PI32;
    
    float forward_x = -sinf(p->yaw);
    float forward_z = cosf(p->yaw);
    
    float input_x = 0.0f;
    float input_y = 0.0f;
    float input_z = 0.0f;
    
    if(keys[SDL_SCANCODE_UP])
    {
        input_x += forward_x;
        input_z += forward_z;
        p->isWalking = true;
        p->isSearching = false;
    }
    if(keys[SDL_SCANCODE_DOWN])
    {
        input_x -= forward_x;
        input_z -= forward_z;
        p->isWalking = true;
        p->isSearching = false;
    }
    
    if(p->isWalking)
    {
        p->velocity.x += input_x * PLAYER_ACCELERATION * dt;
        p->velocity.z += input_z * PLAYER_ACCELERATION * dt;
        
        // Clamp to max speed
        float speed = sqrtf(p->velocity.x * p->velocity.x +
                            p->velocity.z * p->velocity.z);
        
        if(speed > PLAYER_MAX_SPEED)
        {
            float scale = PLAYER_MAX_SPEED / speed;
            p->velocity.x *= scale;
            p->velocity.z *= scale;
        }
    }
    else
    {
        float speed = sqrtf(p->velocity.x * p->velocity.x +
                            p->velocity.z * p->velocity.z);
        
        if(speed > 0.0f)
        {
            float drop = speed * PLAYER_DECELERATION * dt;
            float new_speed = speed - drop;
            if(new_speed < 0.0f) new_speed = 0.0f;
            float scale = new_speed / speed;
            p->velocity.x *= scale;
            p->velocity.z *= scale;
            
        }
    }
    
    if(keys[SDL_SCANCODE_LEFT])
        p->yaw -= 2.0f * dt;
    if(keys[SDL_SCANCODE_RIGHT])
        p->yaw += 2.0f * dt;
    
    p->position.x += p->velocity.x * dt;
    p->position.z += p->velocity.z * dt;
    
    return(true);
}

void
Render(GameState *gamestate, font_atlas *f)
{
    // NOTE(trist007): worth noting that SDL_GetTicks returns an ever increased Uint32
    // so after roughly 49.7 days it wraps back to near zero but for AnimTime it should
    // have much effect
    gamestate->player.AnimTime = SDL_GetTicks() * 0.001f;
    //if(gamestate->model.animCount > 1)
    if(gamestate->player.isWalking)
        sample_animation(&gamestate->model.animations[2],
                         gamestate->player.AnimTime, &gamestate->model.pose);
    else
        sample_animation(&gamestate->model.animations[1],
                         gamestate->player.AnimTime, &gamestate->model.pose);
    eval_pose(&gamestate->model.pose, &gamestate->model.skeleton, gamestate->model.worldMats);
    
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gGPUDevice);
    SDL_GPUTexture *swapchain;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, gSDLWindow, &swapchain, NULL, NULL);
    if(swapchain == NULL)
    {
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }
    
    // draw circle for lightPos
    //PushCircle(&gamestate->po.lp, gamestate->lightPos, 0.25f, 32);
    PushFilledCircle(&gamestate->po.lp, gamestate->lightPos, 0.25f, 50);
    
    // build model * view * projection mvp
    float proj[16], view[16], model[16], vp[16], mvp[16];
    float trans[16], rot[16];
    float identity[16];
    
    mat4_identity(identity);
    
    float aspect = (float)gWINDOW_WIDTH / gWINDOW_HEIGHT;
    
    // orthographic mode
    //mat4_ortho(proj, -aspect, aspect, -1.0f, 1.0f, 0.1f, 100.0f);
    // perspective mode
    mat4_perspective(proj, 45.0f * (3.14159f / 180.0f),
                     (float)gWINDOW_WIDTH / gWINDOW_HEIGHT, 0.1f, 100.0f);
    
    // fixed camera
    //mat4_translation(view, 0.0f, 0.0f, -5.0f); // fixed camera pulled back
    // alone in the dark mode
    
    mat4_lookat(view,
                -0.65f, 7.0f, 11.0f, // camera position
                -0.65f, 2.0f, 4.65f, // look target
                0.0f,   1.0f, 0.0f);   // up vector
    
    // combine proj and view
    mat4_mul(vp, proj, view);  // vp = proj * view
    
    // player - translate then rotate
    mat4_translation(trans, gamestate->player.position.x,
                     gamestate->player.position.y, gamestate->player.position.z); // model moves
    mat4_rotation_y(rot, gamestate->player.yaw);
    mat4_mul(model, trans, rot);
    mat4_mul(mvp, vp, model);  // mvp = vp * model
    RenderRoom(&gamestate->room, &gamestate->lightPos, cmd, swapchain, vp);
    RenderModel(&gamestate->model, &gamestate->player, &gamestate->lightPos, cmd, swapchain, mvp);
    
    /*
    // enemies
    for(int i = 0;
        i < enemyCount;
        i++)
    {
        mat4_translation(trans, enemies[i].x, enemies[i].y, enemies[i].z);
        mat4_rotation_y(rot, enemies[i].rotY);
        mat4_mul(model, trans, rot);
        mat4_mul(mvp, vp, model);
        RenderModel(&enemyModel, ..., cmd, swapchain, mvp);
        }
    */
    
    // Debug text
    char buf[256];
    snprintf(buf, sizeof(buf), "x: %.2f", gamestate->player.position.x);
    DrawText(f, cmd, swapchain, buf, 10.0f, 10.0f, 1.0f, 0.0f, 0.0f);
    snprintf(buf, sizeof(buf), "y: %.2f", gamestate->player.position.y);
    DrawText(f, cmd, swapchain, buf, 10.0f, 30.0f, 1.0f, 0.0f, 0.0f);
    snprintf(buf, sizeof(buf), "z: %.2f", gamestate->player.position.z);
    DrawText(f, cmd, swapchain, buf, 10.0f, 50.0f, 1.0f, 0.0f, 0.0f);
    snprintf(buf, sizeof(buf), "yaw: %.2f", gamestate->player.yaw);
    DrawText(f, cmd, swapchain, buf, 10.0f, 70.0f, 1.0f, 0.0f, 0.0f);
    snprintf(buf, sizeof(buf), "isWalking: %d", gamestate->player.isWalking);
    DrawText(f, cmd, swapchain, buf, 10.0, 90.0f, 1.0f, 0.0f, 0.0f);
    snprintf(buf, sizeof(buf), "Graphics AP: %s", gamestate->graphicsAPI);
    DrawText(f, cmd, swapchain, buf, 10.0f, 110.0f, 1.0f, 0.0f, 0.0f);
    
    
    // wall 0
    PushLine(&gamestate->po.lp, {-7.85f, 0.0f, 6.95f}, {4.88f, 0.0f, 4.82f});
    // wall 1
    PushLine(&gamestate->po.lp, {4.88f, 0.0f, 4.82f}, {4.77f, 0.0f, 9.19f});
    // wall 2
    //PushLine(&po->lp, {4.77f, 0.0f, 9.18f}, {-5.49f, 0.0f, 10.19f});
    
    FlushLines(&gamestate->po.lp, cmd, swapchain, vp);
    
    SDL_SubmitGPUCommandBuffer(cmd);
    
    pose_reset(&gamestate->model.pose, &gamestate->model.skeleton);
}