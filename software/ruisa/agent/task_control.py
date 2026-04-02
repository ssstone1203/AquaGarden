"""
任务运行控制：Web/前端「中断」与本地协作式取消。

在 tasks 循环与关键步骤之间调用 check_cancelled()；收到中断后抛出 TaskCancelled，
由上层结束任务并释放摄像头等资源。
"""

from __future__ import annotations

import threading

_cancel = threading.Event()


class TaskCancelled(Exception):
    """用户请求中断当前任务"""


def begin_task() -> None:
    """新任务开始前清除中断标志。"""
    _cancel.clear()


def request_cancel() -> None:
    """请求中断正在运行的任务（任意线程可调用）。"""
    _cancel.set()


def is_cancel_requested() -> bool:
    return _cancel.is_set()


def check_cancelled() -> None:
    if _cancel.is_set():
        raise TaskCancelled()
