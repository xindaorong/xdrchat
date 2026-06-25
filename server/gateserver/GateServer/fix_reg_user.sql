DROP PROCEDURE IF EXISTS `xdr`.`reg_user`;

DELIMITER //

CREATE DEFINER=`root`@`localhost` PROCEDURE `xdr`.`reg_user`(
    IN `new_name` VARCHAR(255),
    IN `new_email` VARCHAR(255),
    IN `new_pwd` VARCHAR(255),
    OUT `result` INT
)
SQL SECURITY DEFINER
BEGIN
    DECLARE new_id INT DEFAULT 0;

    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        SET result = -1;
    END;

    START TRANSACTION;

    IF EXISTS (SELECT 1 FROM `user` WHERE `name` = new_name) THEN
        SET result = 0;
        COMMIT;
    ELSEIF EXISTS (SELECT 1 FROM `user` WHERE `email` = new_email) THEN
        SET result = 0;
        COMMIT;
    ELSE
        UPDATE `user_id` SET `id` = `id` + 1;
        SELECT `id` INTO new_id FROM `user_id` LIMIT 1;

        INSERT INTO `user` (`uid`, `name`, `email`, `pwd`)
        VALUES (new_id, new_name, new_email, new_pwd);

        SET result = new_id;
        COMMIT;
    END IF;
END //

DELIMITER ;

GRANT EXECUTE ON PROCEDURE `xdr`.`reg_user` TO `root`@`%`;
FLUSH PRIVILEGES;
