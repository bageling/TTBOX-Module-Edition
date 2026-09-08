import pytest

from ttbox_motion.calibration import (
    CalibrationAxis,
    CalibrationObservation,
    CalibrationState,
    CalibrationSession,
    derive_pid_params,
    fit_axis_measurements,
)


def observations(axis, values, delays=None, target_id="track-1"):
    delays = delays or [8.0] * len(values)
    return [
        CalibrationObservation(
            axis=axis,
            injected_count=float(index + 1) * 8,
            measured_delta_px=float(value) * float(index + 1) * 8,
            response_delay_ms=float(delay),
            target_id=target_id,
            valid=True,
        )
        for index, (value, delay) in enumerate(zip(values, delays))
    ]


def test_fit_axis_uses_robust_median_and_rejects_single_spike():
    result = fit_axis_measurements(
        CalibrationAxis.X,
        observations(CalibrationAxis.X, [1.0, 1.05, 0.975, 1.025, 10.0]),
    )
    assert result.converged is True
    assert result.sample_count == 5
    assert 0.9 < result.gain_px_per_count < 1.2
    assert result.rejected_count == 1


def test_fit_axis_rejects_inconsistent_measurements():
    result = fit_axis_measurements(
        CalibrationAxis.Y,
        observations(CalibrationAxis.Y, [2.0, 8.0, 20.0, 40.0, 80.0]),
    )
    assert result.converged is False
    assert "一致性" in result.failure_reason


def test_fit_axis_requires_same_target_identity():
    values = observations(CalibrationAxis.X, [8.0, 8.2, 7.9, 8.1, 8.0])
    values[-1].target_id = "track-2"
    result = fit_axis_measurements(CalibrationAxis.X, values)
    assert result.converged is False
    assert "目标" in result.failure_reason


def test_fit_axis_estimates_delay_from_valid_observations():
    result = fit_axis_measurements(
        CalibrationAxis.X,
        observations(CalibrationAxis.X, [8.0, 8.1, 7.9, 8.0, 8.2], [7.0, 8.0, 9.0, 8.5, 7.5]),
    )
    assert result.converged is True
    assert 7.0 <= result.response_delay_ms <= 9.0


def test_calibration_session_has_explicit_state_transitions():
    session = CalibrationSession()
    assert session.state is CalibrationState.IDLE
    session.start()
    assert session.state is CalibrationState.PREPARING
    session.begin_axis(CalibrationAxis.X)
    assert session.state is CalibrationState.STABILIZE_X
    session.begin_sampling()
    assert session.state is CalibrationState.SAMPLING_X
    session.begin_analysis()
    assert session.state is CalibrationState.ANALYZING_X
    session.complete_axis()
    assert session.state is CalibrationState.STABILIZE_Y
    session.fail("目标不稳定")
    assert session.state is CalibrationState.FAILED
    assert session.failure_reason == "目标不稳定"


def test_derive_pid_params_scales_kp_inverse_to_gain():
    # 增益越大（1 count 移动越多 px），KP 应越小以防过冲
    low_gain = derive_pid_params(0.4, 0.4, 30)
    high_gain = derive_pid_params(1.5, 1.5, 30)
    assert low_gain["kp"] > high_gain["kp"]
    assert 0.0 < low_gain["kp"] <= 60.0


def test_derive_pid_params_increases_kd_with_delay():
    low_delay = derive_pid_params(0.65, 0.65, 10)
    high_delay = derive_pid_params(0.65, 0.65, 60)
    assert high_delay["kd"] > low_delay["kd"]


def test_derive_pid_params_reduces_predict_with_delay():
    low_delay = derive_pid_params(0.65, 0.65, 10)
    high_delay = derive_pid_params(0.65, 0.65, 60)
    assert high_delay["predict"] < low_delay["predict"]
    assert 0.1 <= high_delay["predict"] <= 0.35


def test_derive_pid_params_handles_extreme_gain_delay():
    # 超高增益 + 高延迟：KP 走保守分支，必须仍给出有效参数
    d = derive_pid_params(1.5, 1.5, 60)
    assert 4.0 <= d["kp"] <= 60.0
    assert 4.0 <= d["kd"] <= 50.0
    assert 0.1 <= d["predict"] <= 0.35


def test_derive_pid_params_rejects_zero_gain():
    import pytest as _p

    with _p.raises(ValueError):
        derive_pid_params(0.0, 0.65, 30)
