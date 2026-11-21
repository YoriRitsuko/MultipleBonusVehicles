.intel_syntax noprefix
.text

# Glue between our C++ calls and the game's registers.

.globl _DemoLocatorHook
_DemoLocatorHook:
    cmp dword ptr [edi + 4], 1
    je .Lnormal_parked
    cmp esi, 2
    jne .Lcontinue_locator
    # These flags belong to the demo locators. The stock game ignores them.
    cmp dword ptr [edi + 4], 0x4d425601
    je .Lformula_one_car
    cmp dword ptr [edi + 4], 0x4d425602
    jne .Lcontinue_locator
    mov ecx, offset bart_car
    jmp .Ladd_demo_car
.Lformula_one_car:
    mov ecx, offset formula_one_car
.Ladd_demo_car:
    push ebx
    push ecx
    call [get_parked_car_manager]
    push eax
    call _OnAdd
    add esp, 12
.Lcontinue_locator:
    jmp [continue_locator]
.Lnormal_parked:
    jmp [normal_parked_locator]

.globl _AddHook
_AddHook:
    push dword ptr [esp + 4]
    push ecx
    push edx
    call _OnAdd
    add esp, 12
    ret 4

.globl _NativeAdd
_NativeAdd:
    mov edx, [esp + 4]
    mov ecx, [esp + 8]
    push dword ptr [esp + 12]
    call [add_free_car]
    ret

.globl _RequestHook
_RequestHook:
    push eax
    push dword ptr [esp + 28]
    call _PrepareRequest
    add esp, 4
    pop edx
    test eax, eax
    jz .Lskip_request
    # Both section arguments need to point at this car's own section.
    mov [esp + 16], eax
    mov [esp + 20], eax
    mov eax, edx
    jmp [append_request]
.Lskip_request:
    ret 28

.globl _NativeCreate
_NativeCreate:
    push edi
    mov edi, [esp + 8]
    call [create_free_car]
    pop edi
    ret

.globl _VehicleInitHook
_VehicleInitHook:
    test eax, eax
    jz .Lno_vehicle
    mov esi, eax
    mov edx, [esi]
    mov ecx, esi
    jmp [vehicle_initialized]
.Lno_vehicle:
    pop esi
    jmp [create_epilogue]

.globl _NativeRemove
_NativeRemove:
    push ebx
    mov ebx, [esp + 8]
    call [remove_free_car]
    pop ebx
    ret

.globl _FindHook
_FindHook:
    push eax
    push esi
    push ebx
    push ecx
    call _FindCarInfo
    add esp, 16
    ret

.globl _UnloadHook
_UnloadHook:
    lea eax, [esp + 0x14]
    push eax
    push ebp
    call _OnUnloadZone
    add esp, 8
    jmp [event_epilogue]

.globl _CountHook
_CountHook:
    cmp byte ptr [esp + 0x3c], 0
    jne .Lignore_bonus_husk
    inc dword ptr [ebp + 0x10]
.Lignore_bonus_husk:
    pop edi
    pop esi
    jmp [count_epilogue]

.globl _NearbyHook
_NearbyHook:
    push esi
    push eax
    call _OnRemoveNearby
    add esp, 8
    ret

.globl _DestructorHook
_DestructorHook:
    push ecx
    call _OnDestroy
    add esp, 4
    ret

.globl _NativeDestructor
_NativeDestructor:
    mov ecx, [esp + 4]
    jmp [destroy_manager]

.globl _CancelHook
_CancelHook:
    call _OnCancelLoads
    pop edi
    pop esi
    pop ebx
    ret

.globl _CurrentVehicle
_CurrentVehicle:
    call [get_gameplay_manager]
    test eax, eax
    jz .Lno_current
    jmp [get_current_vehicle]
.Lno_current:
    ret

.globl _AvatarVehicle
_AvatarVehicle:
    call [get_avatar_manager]
    test eax, eax
    jz .Lno_avatar
    mov ecx, eax
    xor eax, eax
    call [get_avatar_for_player]
    test eax, eax
    jz .Lno_avatar
    jmp [get_avatar_vehicle]
.Lno_avatar:
    ret

.globl _IsConstructing
_IsConstructing:
    push esi
    mov esi, [esp + 8]
    call [get_vehicle_central]
    test eax, eax
    jz .Lno_construction
    call [is_under_construction]
.Lno_construction:
    pop esi
    ret

.section .rdata,"dr"
.p2align 2
get_parked_car_manager:  .long 0x508f30
normal_parked_locator:   .long 0x43ee46
continue_locator:        .long 0x43ee5e
add_free_car:            .long 0x5099d0
append_request:          .long 0x43e410
create_free_car:         .long 0x509f00
vehicle_initialized:     .long 0x509f2a
create_epilogue:         .long 0x509faf
remove_free_car:         .long 0x509b00
event_epilogue:          .long 0x509693
count_epilogue:          .long 0x509549
destroy_manager:         .long 0x509df0
get_gameplay_manager:    .long 0x448080
get_current_vehicle:     .long 0x4498e0
get_avatar_manager:      .long 0x4d7a30
get_avatar_for_player:   .long 0x4d7f40
get_avatar_vehicle:      .long 0x4c7800
get_vehicle_central:     .long 0x4dab90
is_under_construction:   .long 0x4dbec0
formula_one_car:         .asciz "fone_v"
bart_car:                .asciz "bart_v"
