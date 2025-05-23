/*
 * Created on Tue Jan 09 2024
 *
 * Copyright 2021 YugaByte, Inc. and Contributors
 * Licensed under the Polyform Free Trial License 1.0.0 (the "License")
 * You may not use this file except in compliance with the License. You may obtain a copy of the License at
 * http://github.com/YugaByte/yugabyte-db/blob/master/licenses/POLYFORM-FREE-TRIAL-LICENSE-1.0.0.txt
 */

import { FC } from 'react';
import { useTranslation } from 'react-i18next';
import { Typography, makeStyles } from '@material-ui/core';
import { useIsTaskNewUIEnabled } from '../TaskUtils';
import { hideTaskInDrawer, showTaskInDrawer } from '../../../../actions/tasks';
import { useDispatch } from 'react-redux';

interface TaskDetailSimpleCompProps {
  taskUUID: string;
  universeUUID?: string;
}

const useStyles = makeStyles((theme) => ({
  viewDetails: {
    color: theme.palette.ybacolors.textInProgress,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    textDecoration: 'underline',
    marginLeft: '8px',
    marginRight: '8px',
    whiteSpace: 'nowrap'
  }
}));

export const TaskDetailSimpleComp: FC<TaskDetailSimpleCompProps> = ({ taskUUID, universeUUID }) => {
  const { t } = useTranslation('translation', {
    keyPrefix: 'taskDetails.simple'
  });

  const dispatch = useDispatch();

  const classes = useStyles();

  const isTaskNewUIEnabled = useIsTaskNewUIEnabled();

  const toggleTaskDetailsDrawer = (flag: boolean) => {
    if (flag) {
      dispatch(showTaskInDrawer(taskUUID));
    } else {
      dispatch(hideTaskInDrawer());
    }
  };

  if (!isTaskNewUIEnabled) return null;

  return (
    <>
      <div
        data-testid="view-task-details"
        onClick={(e) => {
          e.preventDefault();
          e.stopPropagation();
          toggleTaskDetailsDrawer(true);
        }}
        className={classes.viewDetails}
      >
        <Typography variant="body2">{t('viewDetails')}</Typography>
      </div>
    </>
  );
};
