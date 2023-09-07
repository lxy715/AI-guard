  /**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       chassis_behaviour.c/h
  * @brief      according to remote control, change the chassis behaviour.
  *             ����ң������ֵ������������Ϊ��
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. done
  *  V1.1.0     Nov-11-2019     RM              1. add some annotation
  *
  @verbatim
  ==============================================================================    
    ���Ҫ����һ���µ���Ϊģʽ
    1.���ȣ���chassis_behaviour.h�ļ��У� ����һ������Ϊ������ chassis_behaviour_e
    erum
    {  
        ...
        ...
        CHASSIS_XXX_XXX, // �����ӵ�
    }chassis_behaviour_e,

    2. ʵ��һ���µĺ��� chassis_xxx_xxx_control(fp32 *vx, fp32 *vy, fp32 *wz, chassis_move_t * chassis )
        "vx,vy,wz" �����ǵ����˶�����������
        ��һ������: 'vx' ͨ�����������ƶ�,��ֵ ǰ���� ��ֵ ����
        �ڶ�������: 'vy' ͨ�����ƺ����ƶ�,��ֵ ����, ��ֵ ����
        ����������: 'wz' �����ǽǶȿ��ƻ�����ת�ٶȿ���
        ������µĺ���, ���ܸ� "vx","vy",and "wz" ��ֵ��Ҫ���ٶȲ���
    3.  ��"chassis_behaviour_mode_set"��������У������µ��߼��жϣ���chassis_behaviour_mode��ֵ��CHASSIS_XXX_XXX
        �ں����������"else if(chassis_behaviour_mode == CHASSIS_XXX_XXX)" ,Ȼ��ѡ��һ�ֵ��̿���ģʽ
        4��:
        CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW : 'vx' and 'vy'���ٶȿ��ƣ� 'wz'�ǽǶȿ��� ��̨�͵��̵���ԽǶ�
        �����������"xxx_angle_set"������'wz'
        CHASSIS_VECTOR_FOLLOW_CHASSIS_YAW : 'vx' and 'vy'���ٶȿ��ƣ� 'wz'�ǽǶȿ��� ���̵������Ǽ�����ľ��ԽǶ�
        �����������"xxx_angle_set"
        CHASSIS_VECTOR_NO_FOLLOW_YAW : 'vx' and 'vy'���ٶȿ��ƣ� 'wz'����ת�ٶȿ���
        CHASSIS_VECTOR_RAW : ʹ��'vx' 'vy' and 'wz'ֱ�����Լ�������ֵĵ���ֵ������ֵ��ֱ�ӷ��͵�can ������
    4.  ��"chassis_behaviour_control_set" �������������
        else if(chassis_behaviour_mode == CHASSIS_XXX_XXX)
        {
            chassis_xxx_xxx_control(vx_set, vy_set, angle_set, chassis_move_rc_to_vector);
        }
  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */

#include "chassis_behaviour.h"
#include "cmsis_os.h"
#include "chassis_task.h"
#include "arm_math.h"
#include "gimbal_behaviour.h"

//ǰ��վ״̬
#define PERSON_OUTPOST_STATE(event) (event & (1 << PERSON_OUTPOST_STATE_BIT))

/**
  * @brief          ������������Ϊ״̬���£�����ģʽ��raw���ʶ��趨ֵ��ֱ�ӷ��͵�can�����Ϲʶ����趨ֵ������Ϊ0
  * @author         RM
  * @param[in]      vx_setǰ�����ٶ� �趨ֵ��ֱ�ӷ��͵�can������
  * @param[in]      vy_set���ҵ��ٶ� �趨ֵ��ֱ�ӷ��͵�can������
  * @param[in]      wz_set��ת���ٶ� �趨ֵ��ֱ�ӷ��͵�can������
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         ���ؿ�
  */
static void chassis_zero_force_control(fp32 *vx_can_set, fp32 *vy_can_set, fp32 *wz_can_set, chassis_move_t *chassis_move_rc_to_vector);

/**
  * @brief          ���̲��ƶ�����Ϊ״̬���£�����ģʽ�ǲ�����Ƕȣ�
  * @author         RM
  * @param[in]      vx_setǰ�����ٶ�,��ֵ ǰ���ٶȣ� ��ֵ �����ٶ�
  * @param[in]      vy_set���ҵ��ٶ�,��ֵ �����ٶȣ� ��ֵ �����ٶ�
  * @param[in]      wz_set��ת���ٶȣ���ת�ٶ��ǿ��Ƶ��̵ĵ��̽��ٶ�
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         ���ؿ�
  */
static void chassis_no_move_control(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector);

/**
  * @brief          ���̸�����̨����Ϊ״̬���£�����ģʽ�Ǹ�����̨�Ƕȣ�������ת�ٶȻ���ݽǶȲ���������ת�Ľ��ٶ�
  * @author         RM
  * @param[in]      vx_setǰ�����ٶ�,��ֵ ǰ���ٶȣ� ��ֵ �����ٶ�
  * @param[in]      vy_set���ҵ��ٶ�,��ֵ �����ٶȣ� ��ֵ �����ٶ�
  * @param[in]      angle_set��������̨���Ƶ�����ԽǶ�
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         ���ؿ�
  */
static void chassis_infantry_follow_gimbal_yaw_control(fp32 *vx_set, fp32 *vy_set, fp32 *angle_set, chassis_move_t *chassis_move_rc_to_vector);

/**
  * @brief          ���̸������yaw����Ϊ״̬���£�����ģʽ�Ǹ�����̽Ƕȣ�������ת�ٶȻ���ݽǶȲ���������ת�Ľ��ٶ�
  * @author         RM
  * @param[in]      vx_setǰ�����ٶ�,��ֵ ǰ���ٶȣ� ��ֵ �����ٶ�
  * @param[in]      vy_set���ҵ��ٶ�,��ֵ �����ٶȣ� ��ֵ �����ٶ�
  * @param[in]      angle_set�������õ�yaw����Χ -PI��PI
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         ���ؿ�
  */
static void chassis_engineer_follow_chassis_yaw_control(fp32 *vx_set, fp32 *vy_set, fp32 *angle_set, chassis_move_t *chassis_move_rc_to_vector);

/**
  * @brief          ���̲�����Ƕȵ���Ϊ״̬���£�����ģʽ�ǲ�����Ƕȣ�������ת�ٶ��ɲ���ֱ���趨
  * @author         RM
  * @param[in]      vx_setǰ�����ٶ�,��ֵ ǰ���ٶȣ� ��ֵ �����ٶ�
  * @param[in]      vy_set���ҵ��ٶ�,��ֵ �����ٶȣ� ��ֵ �����ٶ�
  * @param[in]      wz_set�������õ���ת�ٶ�,��ֵ ��ʱ����ת����ֵ ˳ʱ����ת
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         ���ؿ�
  */
static void chassis_no_follow_yaw_control(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector);

/**
  * @brief          ���̿�������Ϊ״̬���£�����ģʽ��rawԭ��״̬���ʶ��趨ֵ��ֱ�ӷ��͵�can������
  * @param[in]      vx_setǰ�����ٶ�,��ֵ ǰ���ٶȣ� ��ֵ �����ٶ�
  * @param[in]      vy_set���ҵ��ٶȣ���ֵ �����ٶȣ� ��ֵ �����ٶ�
  * @param[in]      wz_set ��ת�ٶȣ� ��ֵ ��ʱ����ת����ֵ ˳ʱ����ת
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         none
  */

static void chassis_open_set_control(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector);

/**
 * @brief  �����Զ�ģʽ�µ����˶����ƣ����ݶ�ȡ�Ӿ�������ҷ���������з������˵ľ������ݿ��Ƶ����ƶ�
 * 
 * @param vx_set ��ֵ ǰ���ٶ� ��ֵ �����ٶ�
 * @param vy_set ��ֵ �����ٶȣ���ֵ �����ٶ�
 * @param wz_set ��ת�ٶ� ��ֵ ��ʱ����ת ��ֵ ˳ʱ����ת
 * @param chassis_move_vision_to_vector 
 */
static void chassis_auto_follow_target_control(fp32* vx_set, fp32* vy_set, fp32* wz_set, chassis_move_t* chassis_move_vision_to_vector);

/**
 * @brief  �����Զ�ģʽ�µ����˶����ƣ����ݶ�ȡ�Ӿ�������ҷ���������з������˵ľ������ݿ��Ƶ����ƶ�
 * 
 * @param vx_set ��ֵ ǰ���ٶ� ��ֵ �����ٶ�
 * @param vy_set ��ֵ �����ٶȣ���ֵ �����ٶ�
 * @param wz_set ��ת�ٶ� ��ֵ ��ʱ����ת ��ֵ ˳ʱ����ת
 * @param chassis_auto_move_vision_to_vector 
 */
static void chassis_auto_move_control(fp32* vx_set, fp32* vy_set, fp32* wz_set, chassis_move_t* chassis_auto_move_to_vector);

/*----------------------------------�ڲ�����---------------------------*/
//highlight, the variable chassis behaviour mode 
//���⣬���������Ϊģʽ����
chassis_behaviour_e chassis_behaviour_mode = CHASSIS_ZERO_FORCE;



/**
  * @brief          ͨ���߼��жϣ���ֵ"chassis_behaviour_mode"������ģʽ
  * @param[in]      chassis_move_mode: ��������
  * @retval         none
  */
void chassis_behaviour_mode_set(chassis_move_t *chassis_move_mode)
{
    if (chassis_move_mode == NULL)
    {
        return;
    }

    //remote control  set chassis behaviour mode
    //ң��������ģʽ
    if (switch_is_down(chassis_move_mode->chassis_RC->rc.s[CHASSIS_MODE_CHANNEL]))
    {
        // ң���������²൲λΪ��������ģʽ
        chassis_behaviour_mode = CHASSIS_ZERO_FORCE;
    }
    else if (switch_is_mid(chassis_move_mode->chassis_RC->rc.s[CHASSIS_MODE_CHANNEL]))
    {
        // ң�����е�Ϊң��������ģʽ��

        // Ĭ�ϵ��̸�����̨
        chassis_behaviour_mode = CHASSIS_FOLLOW_GIMBAL_YAW;
    }
    else if (switch_is_up(chassis_move_mode->chassis_RC->rc.s[CHASSIS_MODE_CHANNEL]))
    {
        // �ϵ�Ϊ�Զ�ģʽ,�����Զ�����
        //  ����ԭ�ز��� -- �ڱ���Ѫ С����
        judge_chassis_auto_mode(chassis_move_mode);

        // �ж��Զ�ģʽ
        if (switch_is_down(chassis_move_mode->chassis_RC->rc.s[CHASSIS_AUTO_MODE]))
        {
            if (chassis_move_mode->chassis_auto.chassis_auto_mode == CHASSIS_ATTACK)
            {
                chassis_behaviour_mode = CHASSIS_NO_MOVE;
            }
            else if (chassis_move_mode->chassis_auto.chassis_auto_mode == CHASSIS_RUN)
            {
                chassis_behaviour_mode = CHASSIS_SPIN;
            }
        }
        else if (switch_is_mid(chassis_move_mode->chassis_RC->rc.s[CHASSIS_AUTO_MODE]))
        {
            //�ж��Ƿ�Ϊ�Զ��ƶ�ģʽ
            if (judge_cur_mode_is_auto_move_mode())
            {
                chassis_behaviour_mode = CHASSIS_AUTO_MOVE;
            }
            else
            {
                // �����Ӿ������˶������жϵ����Ƿ��ƶ�
                if (chassis_move_mode->chassis_auto.chassis_vision_control_point->vision_control_chassis_mode == FOLLOW_TARGET)
                {
                    chassis_behaviour_mode = CHASSIS_AUTO_FOLLOW_TARGET;
                }
                else if (chassis_move_mode->chassis_auto.chassis_vision_control_point->vision_control_chassis_mode == UNFOLLOW_TARGET)
                {
                    // ����ǰѪ������һ�����������Ѫ������ʼС�����Ա�
                    if (chassis_move_mode->chassis_auto.auto_HP.cur_HP >= chassis_move_mode->chassis_auto.auto_HP.max_HP * HP_ALLOW_PROPORTION)
                    {
                        chassis_behaviour_mode = CHASSIS_NO_MOVE;
                    }
                    else
                    {
                        chassis_behaviour_mode = CHASSIS_SPIN;
                    }
                }
            }
        }
        else if (switch_is_up(chassis_move_mode->chassis_RC->rc.s[CHASSIS_AUTO_MODE]))
        {
            //ԭ��С����
            chassis_behaviour_mode = CHASSIS_SPIN;
        }
    }
    else if (toe_is_error(DBUS_TOE))
    {
        //���ź�
        chassis_behaviour_mode = CHASSIS_ZERO_FORCE;
    }
    else
    {
        chassis_behaviour_mode = CHASSIS_ZERO_FORCE;
    }
    //when gimbal in some mode, such as init mode, chassis must's move
    //����̨��ĳЩģʽ�£����ʼ���� ���̲���
    if (gimbal_cmd_to_chassis_stop())
    {
        chassis_behaviour_mode = CHASSIS_NO_MOVE;
    }

    //��ֹ���̸�����̨����̨��������ƶ�����̨�������,���̲��ƶ�
    if (toe_is_error(YAW_GIMBAL_MOTOR_TOE) && toe_is_error(PITCH_GIMBAL_MOTOR_TOE))
    {
        chassis_behaviour_mode = CHASSIS_NO_MOVE;
    }

    //accord to beheviour mode, choose chassis control mode
    //������Ϊģʽѡ��һ�����̿���ģʽ
    if (chassis_behaviour_mode == CHASSIS_ZERO_FORCE)
    {
        chassis_move_mode->chassis_mode = CHASSIS_VECTOR_RAW;
    }
    else if (chassis_behaviour_mode == CHASSIS_NO_MOVE)
    {
        chassis_move_mode->chassis_mode = CHASSIS_VECTOR_NO_FOLLOW_YAW;
    }
    else if (chassis_behaviour_mode == CHASSIS_FOLLOW_GIMBAL_YAW)
    {
        chassis_move_mode->chassis_mode = CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW;
    }
    else if (chassis_behaviour_mode == CHASSIS_ENGINEER_FOLLOW_CHASSIS_YAW)
    {
        chassis_move_mode->chassis_mode = CHASSIS_VECTOR_FOLLOW_CHASSIS_YAW;
    }
    else if (chassis_behaviour_mode == CHASSIS_NO_FOLLOW_YAW)
    {
        chassis_move_mode->chassis_mode = CHASSIS_VECTOR_NO_FOLLOW_YAW;
    }
    else if (chassis_behaviour_mode == CHASSIS_OPEN)
    {
        chassis_move_mode->chassis_mode = CHASSIS_VECTOR_RAW;
    }
    else if (chassis_behaviour_mode == CHASSIS_SPIN)
    {
        chassis_move_mode->chassis_mode = CHASSIS_VECTOR_SPIN;
    }
    else if (chassis_behaviour_mode == CHASSIS_AUTO_FOLLOW_TARGET)
    {
        chassis_move_mode->chassis_mode = CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW;
    }
    else if (chassis_behaviour_mode == CHASSIS_AUTO_MOVE)
    {
        chassis_move_mode->chassis_mode = CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW;
    }
}

void judge_chassis_auto_mode(chassis_move_t* chassis_judge_auto_mode)
{   
    //װ�װ屻�����־
    static bool_t hit_flag = 0; 
    //δ�������ʱ��
    static uint16_t not_hit_time = 0;
    //��ȡѪ��
    chassis_judge_auto_mode->chassis_auto.auto_HP.max_HP = chassis_judge_auto_mode->chassis_auto.ext_game_robot_state_point->max_HP;
    chassis_judge_auto_mode->chassis_auto.auto_HP.last_HP = chassis_judge_auto_mode->chassis_auto.auto_HP.cur_HP; 
    chassis_judge_auto_mode->chassis_auto.auto_HP.cur_HP = chassis_judge_auto_mode->chassis_auto.ext_game_robot_state_point->remain_HP;

    //�ж�Ѫ���Ƿ����
    if ((chassis_judge_auto_mode->chassis_auto.auto_HP.cur_HP - chassis_judge_auto_mode->chassis_auto.auto_HP.last_HP) < 0)
    {
        //Ѫ������

        //�ж��Ƿ�Ϊװ�װ������������
        if (chassis_judge_auto_mode->chassis_auto.ext_robot_hurt_point->hurt_type == ARMOR_HURT)
        {
            //��־������
            hit_flag = 1;
            //δ������ʱ�����
            not_hit_time = 0;
            //������װ�װ����������������������Ϊ����
            chassis_judge_auto_mode->chassis_auto.chassis_auto_mode = CHASSIS_RUN;
        }
        else
        {
            // �����򱣳�֮ǰ��״̬
            ; 
        }

    }
    else
    {
        //�ж��Ƿ񱻻����
        if(hit_flag == 1)
        {
            //�������,��ʱ
            not_hit_time ++;

            //�жϾ����ϴλ�����ʱ���Ƿ񵽴ﰲȫʱ��
            if (not_hit_time >= SAFE_TIME)
            {
                // �����־λ����
                hit_flag = 0;

                //��������ΪϮ��ģʽ
                chassis_judge_auto_mode->chassis_auto.chassis_auto_mode = CHASSIS_ATTACK;
            }
            else
            {
                //����Ϊ����ģʽ
                chassis_judge_auto_mode->chassis_auto.chassis_auto_mode = CHASSIS_RUN;
            }

        }
        else
        {
            //δ������
            //�����־λ����
            hit_flag = 0;
            // Ϯ��ģʽ
            chassis_judge_auto_mode->chassis_auto.chassis_auto_mode = CHASSIS_ATTACK;
        }
    }
}

/**
  * @brief          set control set-point. three movement param, according to difference control mode,
  *                 will control corresponding movement.in the function, usually call different control function.
  * @param[out]     vx_set, usually controls vertical speed.
  * @param[out]     vy_set, usually controls horizotal speed.
  * @param[out]     wz_set, usually controls rotation speed.
  * @param[in]      chassis_move_rc_to_vector,  has all data of chassis
  * @retval         none
  */
/**
  * @brief          ���ÿ�����.���ݲ�ͬ���̿���ģʽ��������������Ʋ�ͬ�˶�.������������棬����ò�ͬ�Ŀ��ƺ���.
  * @param[out]     vx_set, ͨ�����������ƶ�.
  * @param[out]     vy_set, ͨ�����ƺ����ƶ�.
  * @param[out]     wz_set, ͨ��������ת�˶�.
  * @param[in]      chassis_move_rc_to_vector,  ��������������Ϣ.
  * @retval         none
  */

void chassis_behaviour_control_set(fp32 *vx_set, fp32 *vy_set, fp32 *angle_set, chassis_move_t *chassis_move_rc_to_vector)
{

    if (vx_set == NULL || vy_set == NULL || angle_set == NULL || chassis_move_rc_to_vector == NULL)
    {
        return;
    }

    if (chassis_behaviour_mode == CHASSIS_ZERO_FORCE)
    {
        chassis_zero_force_control(vx_set, vy_set, angle_set, chassis_move_rc_to_vector);
    }
    else if (chassis_behaviour_mode == CHASSIS_NO_MOVE)
    {
        chassis_no_move_control(vx_set, vy_set, angle_set, chassis_move_rc_to_vector);
    }
    else if (chassis_behaviour_mode == CHASSIS_FOLLOW_GIMBAL_YAW) // ������̨
    {
        chassis_infantry_follow_gimbal_yaw_control(vx_set, vy_set, angle_set, chassis_move_rc_to_vector);
    }
    else if (chassis_behaviour_mode == CHASSIS_SPIN) // С����
    {
        chassis_infantry_follow_gimbal_yaw_control(vx_set, vy_set, angle_set, chassis_move_rc_to_vector);
    }
    else if (chassis_behaviour_mode == CHASSIS_ENGINEER_FOLLOW_CHASSIS_YAW)
    {
        chassis_engineer_follow_chassis_yaw_control(vx_set, vy_set, angle_set, chassis_move_rc_to_vector);
    }
    else if (chassis_behaviour_mode == CHASSIS_NO_FOLLOW_YAW)
    {
        chassis_no_follow_yaw_control(vx_set, vy_set, angle_set, chassis_move_rc_to_vector);
    }
    else if (chassis_behaviour_mode == CHASSIS_OPEN)
    {
        chassis_open_set_control(vx_set, vy_set, angle_set, chassis_move_rc_to_vector);
    }
    else if (chassis_behaviour_mode == CHASSIS_AUTO_FOLLOW_TARGET)
    {
        chassis_auto_follow_target_control(vx_set, vy_set, angle_set, chassis_move_rc_to_vector);
    }
    else if (chassis_behaviour_mode == CHASSIS_AUTO_MOVE)
    {
        chassis_auto_move_control(vx_set, vy_set, angle_set, chassis_move_rc_to_vector);
    }
}

/**
  * @brief          when chassis behaviour mode is CHASSIS_ZERO_FORCE, the function is called
  *                 and chassis control mode is raw. The raw chassis chontrol mode means set value
  *                 will be sent to CAN bus derectly, and the function will set all speed zero.
  * @param[out]     vx_can_set: vx speed value, it will be sent to CAN bus derectly.
  * @param[out]     vy_can_set: vy speed value, it will be sent to CAN bus derectly.
  * @param[out]     wz_can_set: wz rotate speed value, it will be sent to CAN bus derectly.
  * @param[in]      chassis_move_rc_to_vector: chassis data
  * @retval         none
  */
/**
  * @brief          ������������Ϊ״̬���£�����ģʽ��raw���ʶ��趨ֵ��ֱ�ӷ��͵�can�����Ϲʶ����趨ֵ������Ϊ0
  * @author         RM
  * @param[in]      vx_setǰ�����ٶ� �趨ֵ��ֱ�ӷ��͵�can������
  * @param[in]      vy_set���ҵ��ٶ� �趨ֵ��ֱ�ӷ��͵�can������
  * @param[in]      wz_set��ת���ٶ� �趨ֵ��ֱ�ӷ��͵�can������
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         ���ؿ�
  */

static void chassis_zero_force_control(fp32 *vx_can_set, fp32 *vy_can_set, fp32 *wz_can_set, chassis_move_t *chassis_move_rc_to_vector)
{
    if (vx_can_set == NULL || vy_can_set == NULL || wz_can_set == NULL || chassis_move_rc_to_vector == NULL)
    {
        return;
    }
    *vx_can_set = 0.0f;
    *vy_can_set = 0.0f;
    *wz_can_set = 0.0f;
}

/**
  * @brief          when chassis behaviour mode is CHASSIS_NO_MOVE, chassis control mode is speed control mode.
  *                 chassis does not follow gimbal, and the function will set all speed zero to make chassis no move
  * @param[out]     vx_set: vx speed value, positive value means forward speed, negative value means backward speed,
  * @param[out]     vy_set: vy speed value, positive value means left speed, negative value means right speed.
  * @param[out]     wz_set: wz rotate speed value, positive value means counterclockwise , negative value means clockwise.
  * @param[in]      chassis_move_rc_to_vector: chassis data
  * @retval         none
  */
/**
  * @brief          ���̲��ƶ�����Ϊ״̬���£�����ģʽ�ǲ�����Ƕȣ�
  * @author         RM
  * @param[in]      vx_setǰ�����ٶ�,��ֵ ǰ���ٶȣ� ��ֵ �����ٶ�
  * @param[in]      vy_set���ҵ��ٶ�,��ֵ �����ٶȣ� ��ֵ �����ٶ�
  * @param[in]      wz_set��ת���ٶȣ���ת�ٶ��ǿ��Ƶ��̵ĵ��̽��ٶ�
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         ���ؿ�
  */

static void chassis_no_move_control(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector)
{
    if (vx_set == NULL || vy_set == NULL || wz_set == NULL || chassis_move_rc_to_vector == NULL)
    {
        return;
    }
    *vx_set = 0.0f;
    *vy_set = 0.0f;
    *wz_set = 0.0f;
}

/**
  * @brief          ���̸�����̨����Ϊ״̬���£�����ģʽ�Ǹ�����̨�Ƕȣ�������ת�ٶȻ���ݽǶȲ���������ת�Ľ��ٶ�
  * @author         RM
  * @param[in]      vx_setǰ�����ٶ�,��ֵ ǰ���ٶȣ� ��ֵ �����ٶ�
  * @param[in]      vy_set���ҵ��ٶ�,��ֵ �����ٶȣ� ��ֵ �����ٶ�
  * @param[in]      angle_set��������̨���Ƶ�����ԽǶ�
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         ���ؿ�
  */
static void chassis_infantry_follow_gimbal_yaw_control(fp32 *vx_set, fp32 *vy_set, fp32 *angle_set, chassis_move_t *chassis_move_rc_to_vector)
{
    if (vx_set == NULL || vy_set == NULL || angle_set == NULL || chassis_move_rc_to_vector == NULL)
    {
        return;
    }

    //channel value and keyboard value change to speed set-point, in general
    //ң������ͨ��ֵ�Լ����̰��� �ó� һ������µ��ٶ��趨ֵ
    chassis_rc_to_control_vector(vx_set, vy_set, chassis_move_rc_to_vector);
    *angle_set = 0;
}

/**
  * @brief          when chassis behaviour mode is CHASSIS_ENGINEER_FOLLOW_CHASSIS_YAW, chassis control mode is speed control mode.
  *                 chassis will follow chassis yaw, chassis rotation speed is calculated from the angle difference between set angle and chassis yaw.
  * @param[out]     vx_set: vx speed value, positive value means forward speed, negative value means backward speed,
  * @param[out]     vy_set: vy speed value, positive value means left speed, negative value means right speed.
  * @param[out]     angle_set: control angle[-PI, PI]
  * @param[in]      chassis_move_rc_to_vector: chassis data
  * @retval         none
  */
/**
  * @brief          ���̸������yaw����Ϊ״̬���£�����ģʽ�Ǹ�����̽Ƕȣ�������ת�ٶȻ���ݽǶȲ���������ת�Ľ��ٶ�
  * @author         RM
  * @param[in]      vx_setǰ�����ٶ�,��ֵ ǰ���ٶȣ� ��ֵ �����ٶ�
  * @param[in]      vy_set���ҵ��ٶ�,��ֵ �����ٶȣ� ��ֵ �����ٶ�
  * @param[in]      angle_set�������õ�yaw����Χ -PI��PI
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         ���ؿ�
  */

static void chassis_engineer_follow_chassis_yaw_control(fp32 *vx_set, fp32 *vy_set, fp32 *angle_set, chassis_move_t *chassis_move_rc_to_vector)
{
    if (vx_set == NULL || vy_set == NULL || angle_set == NULL || chassis_move_rc_to_vector == NULL)
    {
        return;
    }

    chassis_rc_to_control_vector(vx_set, vy_set, chassis_move_rc_to_vector);

    *angle_set = rad_format(chassis_move_rc_to_vector->chassis_yaw_set - CHASSIS_ANGLE_Z_RC_SEN * chassis_move_rc_to_vector->chassis_RC->rc.ch[CHASSIS_WZ_CHANNEL]);
}

/**
  * @brief          when chassis behaviour mode is CHASSIS_NO_FOLLOW_YAW, chassis control mode is speed control mode.
  *                 chassis will no follow angle, chassis rotation speed is set by wz_set.
  * @param[out]     vx_set: vx speed value, positive value means forward speed, negative value means backward speed,
  * @param[out]     vy_set: vy speed value, positive value means left speed, negative value means right speed.
  * @param[out]     wz_set: rotation speed,positive value means counterclockwise , negative value means clockwise
  * @param[in]      chassis_move_rc_to_vector: chassis data
  * @retval         none
  */
/**
  * @brief          ���̲�����Ƕȵ���Ϊ״̬���£�����ģʽ�ǲ�����Ƕȣ�������ת�ٶ��ɲ���ֱ���趨
  * @author         RM
  * @param[in]      vx_setǰ�����ٶ�,��ֵ ǰ���ٶȣ� ��ֵ �����ٶ�	
  * @param[in]      vy_set���ҵ��ٶ�,��ֵ �����ٶȣ� ��ֵ �����ٶ�
  * @param[in]      wz_set�������õ���ת�ٶ�,��ֵ ��ʱ����ת����ֵ ˳ʱ����ת
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         ���ؿ�
  */

static void chassis_no_follow_yaw_control(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector)
{
    if (vx_set == NULL || vy_set == NULL || wz_set == NULL || chassis_move_rc_to_vector == NULL)
    {
        return;
    }

    chassis_rc_to_control_vector(vx_set, vy_set, chassis_move_rc_to_vector);
    *wz_set = -CHASSIS_WZ_RC_SEN * chassis_move_rc_to_vector->chassis_RC->rc.ch[CHASSIS_WZ_CHANNEL];
}

/**
  * @brief          when chassis behaviour mode is CHASSIS_OPEN, chassis control mode is raw control mode.
  *                 set value will be sent to can bus.
  * @param[out]     vx_set: vx speed value, positive value means forward speed, negative value means backward speed,
  * @param[out]     vy_set: vy speed value, positive value means left speed, negative value means right speed.
  * @param[out]     wz_set: rotation speed,positive value means counterclockwise , negative value means clockwise
  * @param[in]      chassis_move_rc_to_vector: chassis data
  * @retval         none
  */
/**
  * @brief          ���̿�������Ϊ״̬���£�����ģʽ��rawԭ��״̬���ʶ��趨ֵ��ֱ�ӷ��͵�can������
  * @param[in]      vx_setǰ�����ٶ�,��ֵ ǰ���ٶȣ� ��ֵ �����ٶ�
  * @param[in]      vy_set���ҵ��ٶȣ���ֵ �����ٶȣ� ��ֵ �����ٶ�
  * @param[in]      wz_set ��ת�ٶȣ� ��ֵ ��ʱ����ת����ֵ ˳ʱ����ת
  * @param[in]      chassis_move_rc_to_vector��������
  * @retval         none
  */

static void chassis_open_set_control(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector)
{
    if (vx_set == NULL || vy_set == NULL || wz_set == NULL || chassis_move_rc_to_vector == NULL)
    {
        return;
    }

    *vx_set = chassis_move_rc_to_vector->chassis_RC->rc.ch[CHASSIS_X_CHANNEL] * CHASSIS_OPEN_RC_SCALE;
    *vy_set = -chassis_move_rc_to_vector->chassis_RC->rc.ch[CHASSIS_Y_CHANNEL] * CHASSIS_OPEN_RC_SCALE;
    *wz_set = -chassis_move_rc_to_vector->chassis_RC->rc.ch[CHASSIS_WZ_CHANNEL] * CHASSIS_OPEN_RC_SCALE;
    return;
}


static void chassis_auto_follow_target_control(fp32* vx_set, fp32* vy_set, fp32* wz_set, chassis_move_t* chassis_move_vision_to_vector)
{
    if (vx_set == NULL || vy_set == NULL || wz_set == NULL || chassis_move_vision_to_vector == NULL)
    {
        return;
    }
    chassis_vision_to_control_vector(vx_set, vy_set, chassis_move_vision_to_vector);
    *wz_set = 0;

}

static void chassis_auto_move_control(fp32* vx_set, fp32* vy_set, fp32* wz_set, chassis_move_t* chassis_auto_move_to_vector)
{
    chassis_auto_move_control_vector(vx_set, vy_set, chassis_auto_move_to_vector);
    *wz_set = 0;
}

